/**
 * @file network.cpp
 * @brief Implements the Network class for managing Wi-Fi connections and TCP/UDP communication on the Raspberry Pi Pico W using lwIP.
 * @author Felix Brodmann
 * @date 2026-04-27
 * @version 1.0.0
 */

#include "network.hpp"

Network::~Network() {
    if (is_connected_to_station_wifi) {
        cyw43_arch_deinit();
    }
    if (is_access_point_active) {
        (void)dhcp_server.stop();
        cyw43_arch_deinit();
    }
    if (attached_dns_server) {
        (void)attached_dns_server->stop();
    }
}

err_t Network::tcp_dial_connected_callback(void* arg, tcp_pcb* pcb, err_t err) {
    TcpDialState* state = static_cast<TcpDialState*>(arg);
    state->done = true;
    state->error = err;

    if (err == ERR_OK) {
        state->success = true;
        state->connected_pcb = pcb;
    }
    return ERR_OK;
}

std::optional<Error> Network::initialize() {
    if (is_initialized) {
        return std::nullopt;
    }
    if (cyw43_arch_init()) {
        return Error("cyw43_arch_init failed");
    }
    is_initialized = true;
    return std::nullopt;
}

std::optional<Error> Network::connect_to_wifi_station(const std::string& ssid, const std::string& password, uint32_t timeout_ms) {
    if (!is_initialized) {
        return Error("Network not initialized");
    }
    if (is_access_point_active) {
        return Error("Cannot connect to WiFi station while in AP mode");
    }
    if (is_connected_to_station_wifi) {
        return Error("Already connected to WiFi station");
    }

    cyw43_arch_enable_sta_mode();

    int result = cyw43_arch_wifi_connect_timeout_ms(
        ssid.c_str(),
        password.c_str(),
        CYW43_AUTH_WPA2_AES_PSK,
        timeout_ms
    );

    if (result != 0) {
        return Error("WiFi connect failed");
    }

    is_connected_to_station_wifi = true;
    return std::nullopt;
}

std::optional<Error> Network::setup_wifi_access_point(const std::string& ssid, const std::string& password) {
    if (!is_initialized) {
        return Error("Network not initialized");
    }
    if (is_connected_to_station_wifi) {
        return Error("Cannot set up WiFi access point while in station mode");
    }
    if (is_access_point_active) {
        return Error("Already in AP mode");
    }

    cyw43_arch_enable_ap_mode(
        ssid.c_str(),
        password.c_str(),
        CYW43_AUTH_WPA2_AES_PSK
    );

    auto dhcp_err = dhcp_server.start("192.168.4.1", "255.255.255.0");
    if (dhcp_err) {
        return dhcp_err;
    }

    is_access_point_active = true;
    return std::nullopt;
}

std::optional<Error> Network::add_dns_server(DnsServer& dns_server)
{
    if (!is_initialized) {
        return Error("Network not initialized");
    }

    if (!is_access_point_active) {
        return Error("DNS server requires access point mode");
    }

    ip_addr_t bind_ip;
    std::memset(&bind_ip, 0, sizeof(bind_ip));

    cyw43_arch_lwip_begin();
    const ip4_addr_t* ip = netif_ip_addr4(netif_default);
    if (ip == nullptr) {
        cyw43_arch_lwip_end();
        return Error("No local IP address");
    }

    ip_addr_copy_from_ip4(bind_ip, *ip);
    cyw43_arch_lwip_end();

    auto err = dns_server.start(bind_ip);
    if (err) {
        return err;
    }

    attached_dns_server = &dns_server;
    return std::nullopt;
}

std::tuple<std::unique_ptr<Connection>, std::optional<Error>> Network::dial_tcp(const Address& address, uint32_t timeout_ms) {
    
    if (!is_initialized) {
        return {nullptr, Error("Network not initialized")};
    }
    if (!is_ready()) {
        return {nullptr, Error("Network not ready")};
    }

    ip_addr_t remote_address;
    bool is_valid_ipv4 = ip4addr_aton(address.ip.c_str(), ip_2_ip4(&remote_address));
    if (!is_valid_ipv4) {
        return {nullptr, Error("Invalid IPv4 address format")};
    }
    IP_SET_TYPE_VAL(remote_address, IPADDR_TYPE_V4);

    cyw43_arch_lwip_begin();

    tcp_pcb* pcb = tcp_new();
    if (pcb == nullptr) {
        cyw43_arch_lwip_end();
        return {nullptr, Error("tcp_new failed")};
    }

    TcpDialState state;
    tcp_arg(pcb, &state);

    err_t err = tcp_connect(pcb, &remote_address, address.port, tcp_dial_connected_callback);
    cyw43_arch_lwip_end();

    auto cleanup = [pcb](const std::string& error_message) -> std::tuple<std::unique_ptr<Connection>, std::optional<Error>> {
        if (pcb != nullptr) {
            cyw43_arch_lwip_begin();
            tcp_arg(pcb, nullptr);
            err_t close_error = tcp_close(pcb);
            if (close_error != ERR_OK) {
                tcp_abort(pcb);
            }
            cyw43_arch_lwip_end();
        }
        return {nullptr, Error(std::move(error_message))};
    };

    if (err != ERR_OK) {
        return cleanup("tcp_connect failed");
    }

    absolute_time_t until = make_timeout_time_ms(timeout_ms);

    while (!state.done) {
        #if PICO_CYW43_ARCH_POLL
            cyw43_arch_poll();
        #endif
        sleep_ms(10);

        if (absolute_time_diff_us(get_absolute_time(), until) <= 0) {
            return cleanup("tcp connect timeout");
        }
    }

    if (!state.success || state.connected_pcb == nullptr) {
        return cleanup("tcp connect failed");
    }

    std::string local_ip = "";
    uint16_t local_port = 0;

    cyw43_arch_lwip_begin();
    const ip4_addr_t* ip = netif_ip_addr4(netif_default);
    if (ip != nullptr) {
        local_ip = ipaddr_ntoa(ip);
    }
    local_port = state.connected_pcb->local_port;
    cyw43_arch_lwip_end();

    Address local_addr{"tcp", local_ip, local_port};
    Address remote_addr = address;

    auto conn = std::unique_ptr<TcpConnection>(
        new TcpConnection(state.connected_pcb, std::move(local_addr), std::move(remote_addr))
    );

    conn->attach_callbacks();

    return {std::move(conn), std::nullopt};
}

std::tuple<std::unique_ptr<Listener>, std::optional<Error>> Network::listen_tcp(uint16_t port) {
    if (!is_initialized) {
        return {nullptr, Error("Network not initialized")};
    }

    if (!is_ready()) {
        return {nullptr, Error("Network not ready")};
    }

    cyw43_arch_lwip_begin();

    tcp_pcb* pcb = tcp_new();
    if (pcb == nullptr) {
        cyw43_arch_lwip_end();
        return {nullptr, Error("tcp_new failed")};
    }

    ip_addr_t bind_addr;
    IP_ADDR4(&bind_addr, 0, 0, 0, 0);
    IP_SET_TYPE_VAL(bind_addr, IPADDR_TYPE_V4);

    err_t err = tcp_bind(pcb, &bind_addr, port);
    if (err != ERR_OK) {
        cyw43_arch_lwip_end();
        tcp_abort(pcb);
        return {nullptr, Error("tcp_bind failed")};
    }

    pcb = tcp_listen_with_backlog(pcb, 4);
    if (pcb == nullptr) {
        cyw43_arch_lwip_end();
        tcp_abort(pcb);
        return {nullptr, Error("tcp_listen_with_backlog failed")};
    }

    std::string local_ip = "";
    const ip4_addr_t* ip = netif_ip_addr4(netif_default);
    if (ip != nullptr) {
        local_ip = ipaddr_ntoa(ip);
    }

    cyw43_arch_lwip_end();

    Address local_addr{"tcp", local_ip, port};

    auto listener = std::unique_ptr<TcpListener>(
        new TcpListener(pcb, std::move(local_addr))
    );

    listener->attach_callbacks();

    return {std::move(listener), std::nullopt};
}

std::tuple<std::unique_ptr<Connection>, std::optional<Error>> Network::dial_udp(const Address& address) {
    if (!is_initialized) {
        return {nullptr, Error("Network not initialized")};
    }

    if (!is_ready()) {
        return {nullptr, Error("Network not ready")};
    }

    ip_addr_t remote_ip;
    bool is_valid_ipv4 = ip4addr_aton(address.ip.c_str(), ip_2_ip4(&remote_ip));
    if (!is_valid_ipv4) {
        return {nullptr, Error("Invalid IPv4 address format")};
    }
    IP_SET_TYPE_VAL(remote_ip, IPADDR_TYPE_V4);

    cyw43_arch_lwip_begin();

    udp_pcb* pcb = udp_new();
    if (pcb == nullptr) {
        cyw43_arch_lwip_end();
        return {nullptr, Error("udp_new failed")};
    }

    err_t err = udp_connect(pcb, &remote_ip, address.port);
    if (err != ERR_OK) {
        udp_remove(pcb);
        cyw43_arch_lwip_end();
        return {nullptr, Error("udp_connect failed")};
    }

    std::string local_ip = "";
    uint16_t local_port = pcb->local_port;

    const ip4_addr_t* ip = netif_ip_addr4(netif_default);
    if (ip != nullptr) {
        local_ip = ipaddr_ntoa(ip);
    }

    cyw43_arch_lwip_end();

    Address local_addr{"udp", local_ip, local_port};
    Address remote_addr = address;

    auto conn = std::unique_ptr<UdpConnection>(
        new UdpConnection(pcb, std::move(local_addr), std::move(remote_addr))
    );

    conn->attach_callbacks();

    return {std::move(conn), std::nullopt};
}

std::tuple<std::unique_ptr<Listener>, std::optional<Error>> Network::listen_udp(uint16_t port) {
    if (!is_initialized) {
        return {nullptr, Error("Network not initialized")};
    }

    if (!is_ready()) {
        return {nullptr, Error("Network not ready")};
    }

    cyw43_arch_lwip_begin();

    udp_pcb* pcb = udp_new();
    if (pcb == nullptr) {
        cyw43_arch_lwip_end();
        return {nullptr, Error("udp_new failed")};
    }

    ip_addr_t bind_addr;
    IP_ADDR4(&bind_addr, 0, 0, 0, 0);
    IP_SET_TYPE_VAL(bind_addr, IPADDR_TYPE_V4);

    err_t err = udp_bind(pcb, &bind_addr, port);
    if (err != ERR_OK) {
        udp_remove(pcb);
        cyw43_arch_lwip_end();
        return {nullptr, Error("udp_bind failed")};
    }

    std::string local_ip = "";

    const ip4_addr_t* ip = netif_ip_addr4(netif_default);
    if (ip != nullptr) {
        local_ip = ipaddr_ntoa(ip);
    }

    cyw43_arch_lwip_end();

    Address local_addr{"udp", local_ip, port};

    auto listener = std::unique_ptr<UdpListener>(
        new UdpListener(pcb, std::move(local_addr))
    );

    listener->attach_callbacks();

    return {std::move(listener), std::nullopt};
}