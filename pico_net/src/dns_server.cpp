/**
 * @file dns_server.cpp
 * @brief Implements a simple DNS server using lwIP on the Raspberry Pi Pico W.
 * @author Felix Brodmann
 * @date 2026-04-27
 * @version 1.0.0
 */

#include "dns_server.hpp"

DnsServer::~DnsServer() {
    (void)stop();
}

std::string DnsServer::normalize_host(std::string host) {
    std::transform(host.begin(), host.end(), host.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    while (!host.empty() && host.back() == '.') {
        host.pop_back();
    }

    return host;
}

bool DnsServer::parse_ipv4(const std::string& ip, ip4_addr_t& out) {
    return ip4addr_aton(ip.c_str(), &out) != 0;
}

std::optional<Error> DnsServer::add_record(const std::string& host, const std::string& ip) {
    if (host.empty()) {
        return Error("host is empty");
    }

    ip4_addr_t parsed_ip;
    if (!parse_ipv4(ip, parsed_ip)) {
        return Error("invalid IPv4 address");
    }

    records[normalize_host(host)] = parsed_ip;
    return std::nullopt;
}

std::optional<Error> DnsServer::remove_record(const std::string& host) {
    records.erase(normalize_host(host));
    return std::nullopt;
}

void DnsServer::clear_records() {
    records.clear();
}

std::optional<Error> DnsServer::set_fallback_ip(const std::string& ip) {
    ip4_addr_t parsed_ip;
    if (!parse_ipv4(ip, parsed_ip)) {
        return Error("invalid IPv4 address");
    }

    fallback_ip = parsed_ip;
    return std::nullopt;
}

void DnsServer::clear_fallback_ip() {
    fallback_ip.reset();
}

std::optional<ip4_addr_t> DnsServer::lookup_host(const std::string& host) const {
    auto it = records.find(normalize_host(host));
    if (it != records.end()) {
        return it->second;
    }

    if (fallback_ip.has_value()) {
        return fallback_ip;
    }

    return std::nullopt;
}

std::optional<Error> DnsServer::bind_socket(const ip_addr_t& bind_ip) {
    udp = udp_new();
    if (udp == nullptr) {
        return Error("udp_new failed");
    }

    udp_recv(udp, &DnsServer::dns_recv_callback, this);

    err_t err = udp_bind(udp, &bind_ip, PORT_DNS_SERVER);
    if (err != ERR_OK) {
        udp_remove(udp);
        udp = nullptr;
        return Error("udp_bind failed");
    }

    ip_addr_copy(listen_ip, bind_ip);
    return std::nullopt;
}

std::optional<Error> DnsServer::start(const ip_addr_t& bind_ip) {
    if (udp != nullptr) {
        return Error("dns server already running");
    }

    cyw43_arch_lwip_begin();
    auto err = bind_socket(bind_ip);
    cyw43_arch_lwip_end();

    return err;
}

std::optional<Error> DnsServer::stop() {
    if (udp == nullptr) {
        return std::nullopt;
    }

    cyw43_arch_lwip_begin();
    udp_remove(udp);
    udp = nullptr;
    cyw43_arch_lwip_end();

    return std::nullopt;
}

void DnsServer::dns_recv_callback(void* arg, udp_pcb* upcb, pbuf* p, const ip_addr_t* src_addr, u16_t src_port) {
    if (arg == nullptr) {
        if (p != nullptr) {
            pbuf_free(p);
        }
        return;
    }

    static_cast<DnsServer*>(arg)->handle_request(upcb, p, src_addr, src_port);
}

bool DnsServer::extract_qname(const uint8_t* msg, size_t msg_len, std::string& out_host, const uint8_t*& question_end) const {
    out_host.clear();

    const uint8_t* ptr = msg + sizeof(DnsHeader);
    const uint8_t* end = msg + msg_len;

    bool first = true;
    while (ptr < end) {
        uint8_t label_len = *ptr++;
        if (label_len == 0) {
            break;
        }

        if (label_len > 63) {
            return false;
        }

        if (ptr + label_len > end) {
            return false;
        }

        if (!first) {
            out_host.push_back('.');
        }
        first = false;

        out_host.append(reinterpret_cast<const char*>(ptr), label_len);
        ptr += label_len;
    }

    if (ptr + 4 > end) {
        return false;
    }

    question_end = ptr + 4; // zero label already consumed, plus QTYPE/QCLASS
    out_host = normalize_host(out_host);
    return !out_host.empty();
}

std::optional<Error> DnsServer::send_response(const void* buf, size_t len, const ip_addr_t* dest, uint16_t port) {
    if (udp == nullptr) {
        return Error("dns server not running");
    }

    pbuf* p = pbuf_alloc(PBUF_TRANSPORT, static_cast<u16_t>(len), PBUF_RAM);
    if (p == nullptr) {
        return Error("pbuf_alloc failed");
    }

    std::memcpy(p->payload, buf, len);
    err_t err = udp_sendto(udp, p, dest, port);
    pbuf_free(p);

    if (err != ERR_OK) {
        return Error("udp_sendto failed");
    }

    return std::nullopt;
}

void DnsServer::handle_request(udp_pcb* upcb, pbuf* p, const ip_addr_t* src_addr, u16_t src_port) {
    (void)upcb;

    uint8_t dns_msg[MAX_DNS_MSG_SIZE];
    if (p == nullptr) {
        return;
    }

    size_t msg_len = pbuf_copy_partial(p, dns_msg, sizeof(dns_msg), 0);
    if (msg_len < sizeof(DnsHeader)) {
        pbuf_free(p);
        return;
    }

    auto* dns_hdr = reinterpret_cast<DnsHeader*>(dns_msg);

    uint16_t flags = lwip_ntohs(dns_hdr->flags);
    uint16_t question_count = lwip_ntohs(dns_hdr->question_count);

    if (((flags >> 15) & 0x1) != 0) {
        pbuf_free(p);
        return;
    }

    if (((flags >> 11) & 0xf) != 0) {
        pbuf_free(p);
        return;
    }

    if (question_count < 1) {
        pbuf_free(p);
        return;
    }

    std::string host;
    const uint8_t* question_end = nullptr;
    if (!extract_qname(dns_msg, msg_len, host, question_end)) {
        pbuf_free(p);
        return;
    }

    auto resolved = lookup_host(host);
    if (!resolved.has_value()) {
        pbuf_free(p);
        return;
    }

    uint8_t* answer_ptr = dns_msg + (question_end - dns_msg);

    *answer_ptr++ = 0xc0;
    *answer_ptr++ = static_cast<uint8_t>(sizeof(DnsHeader)); // pointer to QNAME start

    *answer_ptr++ = 0;
    *answer_ptr++ = 1; // A record

    *answer_ptr++ = 0;
    *answer_ptr++ = 1; // IN

    *answer_ptr++ = 0;
    *answer_ptr++ = 0;
    *answer_ptr++ = 0;
    *answer_ptr++ = 60; // TTL

    *answer_ptr++ = 0;
    *answer_ptr++ = 4; // IPv4 length

    uint32_t ip_be = ip4_addr_get_u32(&resolved.value());
    std::memcpy(answer_ptr, &ip_be, 4);
    answer_ptr += 4;

    dns_hdr->flags = lwip_htons(
        (1u << 15) | // QR response
        (1u << 10) | // AA authoritative
        (1u << 7)    // RA
    );
    dns_hdr->question_count = lwip_htons(1);
    dns_hdr->answer_record_count = lwip_htons(1);
    dns_hdr->authority_record_count = 0;
    dns_hdr->additional_record_count = 0;

    printf("DNS query for '%s' resolved to %s\n", host.c_str(), ipaddr_ntoa(&resolved.value()));

    auto send_err = send_response(dns_msg, static_cast<size_t>(answer_ptr - dns_msg), src_addr, src_port);
    (void)send_err;

    pbuf_free(p);
}