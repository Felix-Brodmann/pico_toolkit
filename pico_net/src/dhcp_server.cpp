/**
 * @file dhcp_server.cpp
 * @brief Implements a simple DHCP server using lwIP on the Raspberry Pi Pico W.
 * @author Felix Brodmann
 * @date 2026-04-27
 * @version 1.0.0
 */

#include "dhcp_server.hpp"

DhcpServer::~DhcpServer() {
    (void)stop();
}

std::optional<Error> DhcpServer::start(const std::string& ip, const std::string& netmask_str) {
    ip_addr_t parsed_ip;
    ip_addr_t parsed_netmask;

    if (!ip4addr_aton(ip.c_str(), ip_2_ip4(&parsed_ip))) {
        return Error("invalid DHCP server IP address");
    }
    IP_SET_TYPE_VAL(parsed_ip, IPADDR_TYPE_V4);

    if (!ip4addr_aton(netmask_str.c_str(), ip_2_ip4(&parsed_netmask))) {
        return Error("invalid DHCP netmask");
    }
    IP_SET_TYPE_VAL(parsed_netmask, IPADDR_TYPE_V4);

    return start(parsed_ip, parsed_netmask);
}

std::optional<Error> DhcpServer::start(const ip_addr_t& ip, const ip_addr_t& mask) {
    if (udp != nullptr) {
        return Error("DHCP server already running");
    }

    cyw43_arch_lwip_begin();

    ip_addr_copy(server_ip, ip);
    ip_addr_copy(netmask, mask);
    leases = {};

    udp = udp_new();
    if (udp == nullptr) {
        cyw43_arch_lwip_end();
        return Error("udp_new failed");
    }

    udp_recv(udp, &DhcpServer::udp_recv_callback, this);

    err_t err = udp_bind(udp, IP_ANY_TYPE, PORT_DHCP_SERVER);
    if (err != ERR_OK) {
        udp_remove(udp);
        udp = nullptr;
        cyw43_arch_lwip_end();
        return Error("udp_bind failed");
    }

    cyw43_arch_lwip_end();
    return std::nullopt;
}

std::optional<Error> DhcpServer::stop() {
    if (udp == nullptr) {
        return std::nullopt;
    }

    cyw43_arch_lwip_begin();
    udp_remove(udp);
    udp = nullptr;
    cyw43_arch_lwip_end();

    leases = {};
    return std::nullopt;
}

void DhcpServer::udp_recv_callback(void* arg, udp_pcb* upcb, pbuf* p, const ip_addr_t* src_addr, u16_t src_port) {
    if (arg == nullptr) {
        if (p != nullptr) {
            pbuf_free(p);
        }
        return;
    }

    static_cast<DhcpServer*>(arg)->handle_request(upcb, p, src_addr, src_port);
}

uint8_t* DhcpServer::find_option(uint8_t* opt, uint8_t code) {
    for (int i = 0; i < 308 && opt[i] != DHCP_OPT_END;) {
        if (opt[i] == code) {
            return &opt[i];
        }

        if (opt[i] == 0) {
            ++i;
            continue;
        }

        i += 2 + opt[i + 1];
    }

    return nullptr;
}

void DhcpServer::write_option_n(uint8_t** opt, uint8_t code, size_t n, const void* data) {
    auto* o = *opt;
    *o++ = code;
    *o++ = static_cast<uint8_t>(n);
    std::memcpy(o, data, n);
    *opt = o + n;
}

void DhcpServer::write_option_u8(uint8_t** opt, uint8_t code, uint8_t value) {
    auto* o = *opt;
    *o++ = code;
    *o++ = 1;
    *o++ = value;
    *opt = o;
}

void DhcpServer::write_option_u32(uint8_t** opt, uint8_t code, uint32_t value) {
    auto* o = *opt;
    *o++ = code;
    *o++ = 4;
    *o++ = static_cast<uint8_t>(value >> 24);
    *o++ = static_cast<uint8_t>(value >> 16);
    *o++ = static_cast<uint8_t>(value >> 8);
    *o++ = static_cast<uint8_t>(value);
    *opt = o;
}

int DhcpServer::find_or_allocate_lease(const uint8_t* mac) {
    int chosen = DHCPS_MAX_IP;

    for (int i = 0; i < DHCPS_MAX_IP; ++i) {
        if (std::memcmp(leases[i].mac, mac, MAC_LEN) == 0) {
            return i;
        }

        if (chosen == DHCPS_MAX_IP) {
            static constexpr uint8_t zero_mac[MAC_LEN] = {};

            if (std::memcmp(leases[i].mac, zero_mac, MAC_LEN) == 0) {
                chosen = i;
            }

            uint32_t expiry = (static_cast<uint32_t>(leases[i].expiry) << 16) | 0xffff;
            if (static_cast<int32_t>(expiry - cyw43_hal_ticks_ms()) < 0) {
                std::memset(leases[i].mac, 0, MAC_LEN);
                chosen = i;
            }
        }
    }

    return chosen;
}

std::optional<Error> DhcpServer::send_to(netif* nif, const void* buffer, size_t len, uint32_t ip, uint16_t port) {
    if (udp == nullptr) {
        return Error("DHCP server not running");
    }

    if (len > 0xffff) {
        len = 0xffff;
    }

    pbuf* p = pbuf_alloc(PBUF_TRANSPORT, static_cast<u16_t>(len), PBUF_RAM);
    if (p == nullptr) {
        return Error("pbuf_alloc failed");
    }

    std::memcpy(p->payload, buffer, len);

    ip_addr_t dest;
    IP4_ADDR(ip_2_ip4(&dest),
             static_cast<uint8_t>((ip >> 24) & 0xff),
             static_cast<uint8_t>((ip >> 16) & 0xff),
             static_cast<uint8_t>((ip >> 8) & 0xff),
             static_cast<uint8_t>(ip & 0xff));
    IP_SET_TYPE_VAL(dest, IPADDR_TYPE_V4);

    err_t err;
    if (nif != nullptr) {
        err = udp_sendto_if(udp, p, &dest, port, nif);
    } else {
        err = udp_sendto(udp, p, &dest, port);
    }

    pbuf_free(p);

    if (err != ERR_OK) {
        return Error("udp_sendto failed");
    }

    return std::nullopt;
}

void DhcpServer::handle_request(udp_pcb* upcb, pbuf* p, const ip_addr_t* src_addr, u16_t src_port) {
    (void)upcb;
    (void)src_addr;
    (void)src_port;

    if (p == nullptr) {
        return;
    }

    static constexpr size_t DHCP_MIN_SIZE = 240 + 3;

    DhcpMessage dhcp_msg{};

    if (p->tot_len < DHCP_MIN_SIZE) {
        pbuf_free(p);
        return;
    }

    size_t len = pbuf_copy_partial(p, &dhcp_msg, sizeof(dhcp_msg), 0);
    if (len < DHCP_MIN_SIZE) {
        pbuf_free(p);
        return;
    }

    dhcp_msg.op = DHCPOFFER;
    std::memcpy(&dhcp_msg.yiaddr, &ip4_addr_get_u32(ip_2_ip4(&server_ip)), 4);

    uint8_t* opt = reinterpret_cast<uint8_t*>(&dhcp_msg.options);
    opt += 4; // DHCP magic cookie

    uint8_t* msgtype = find_option(opt, DHCP_OPT_MSG_TYPE);
    if (msgtype == nullptr) {
        pbuf_free(p);
        return;
    }

    switch (msgtype[2]) {
        case DHCPDISCOVER: {
            int yi = find_or_allocate_lease(dhcp_msg.chaddr);
            if (yi == DHCPS_MAX_IP) {
                pbuf_free(p);
                return;
            }

            dhcp_msg.yiaddr[3] = DHCPS_BASE_IP + yi;
            write_option_u8(&opt, DHCP_OPT_MSG_TYPE, DHCPOFFER);
            break;
        }

        case DHCPREQUEST: {
            uint8_t* requested_ip_opt = find_option(opt, DHCP_OPT_REQUESTED_IP);
            if (requested_ip_opt == nullptr) {
                pbuf_free(p);
                return;
            }

            if (std::memcmp(requested_ip_opt + 2, &ip4_addr_get_u32(ip_2_ip4(&server_ip)), 3) != 0) {
                pbuf_free(p);
                return;
            }

            uint8_t yi = requested_ip_opt[5] - DHCPS_BASE_IP;
            if (yi >= DHCPS_MAX_IP) {
                pbuf_free(p);
                return;
            }

            static constexpr uint8_t zero_mac[MAC_LEN] = {};

            if (std::memcmp(leases[yi].mac, dhcp_msg.chaddr, MAC_LEN) == 0) {
                // same client renewing same lease
            } else if (std::memcmp(leases[yi].mac, zero_mac, MAC_LEN) == 0) {
                std::memcpy(leases[yi].mac, dhcp_msg.chaddr, MAC_LEN);
            } else {
                pbuf_free(p);
                return;
            }

            leases[yi].expiry =
                static_cast<uint16_t>((cyw43_hal_ticks_ms() + DEFAULT_LEASE_TIME_S * 1000) >> 16);

            dhcp_msg.yiaddr[3] = DHCPS_BASE_IP + yi;
            write_option_u8(&opt, DHCP_OPT_MSG_TYPE, DHCPACK);
            break;
        }

        default:
            pbuf_free(p);
            return;
    }

    write_option_n(&opt, DHCP_OPT_SERVER_ID, 4, &ip4_addr_get_u32(ip_2_ip4(&server_ip)));
    write_option_n(&opt, DHCP_OPT_SUBNET_MASK, 4, &ip4_addr_get_u32(ip_2_ip4(&netmask)));
    write_option_n(&opt, DHCP_OPT_ROUTER, 4, &ip4_addr_get_u32(ip_2_ip4(&server_ip)));
    write_option_n(&opt, DHCP_OPT_DNS, 4, &ip4_addr_get_u32(ip_2_ip4(&server_ip)));
    write_option_u32(&opt, DHCP_OPT_IP_LEASE_TIME, DEFAULT_LEASE_TIME_S);
    *opt++ = DHCP_OPT_END;

    netif* nif = ip_current_input_netif();
    (void)send_to(nif, &dhcp_msg, static_cast<size_t>(opt - reinterpret_cast<uint8_t*>(&dhcp_msg)),
                  0xffffffff, PORT_DHCP_CLIENT);

    pbuf_free(p);
}