/**
 * @file dns_server.hpp
 * @brief Defines the DnsServer class for implementing a simple DNS server on the Raspberry Pi Pico W.
 * @author Felix Brodmann
 * @date 2026-04-27
 * @version 1.0.0
 */

#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <cstring>

#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"
#include "lwip/udp.h"

#include "error.hpp"

class Network;

/**
 * @class DnsServer
 * @brief A simple DNS server implementation for the Raspberry Pi Pico W.
 * @details This class provides methods to start and stop a DNS server that can respond to DNS queries on the local network. It allows adding and removing DNS records that map hostnames to IP addresses, as well as setting a fallback IP address for unresolved queries.
 * @par Example
 * @code{.cpp}
 * #include <cstdio>
 * 
 * #include "dns_server.hpp"
 * #include "network.hpp"
 * 
 * #include "pico/cyw43_arch.h"
 *
 * int main() {
 *     stdio_init_all();
 *
 *     Network network;
 * 
 *     auto err = network.initialize();
 *     if (err) {
 *         printf("Network initialization failed: %s\n", err->message.c_str());
 *         return -1;
 *     }
 * 
 *     printf("Setting up WiFi Access Point...\n");
 *     err = network.setup_wifi_access_point("PicoEcho", "12345678");
 *     if (err) {
 *         printf("Access Point setup failed: %s\n", err->message.c_str());
 *         return -1;
 *     }
 * 
 *     printf("Setting up DNS server...\n");
 *     DnsServer dns_server;
 *     dns_server.set_fallback_ip("192.168.4.1");
 *     dns_server.add_record("example.local", "192.168.4.2");
 *     err = network.add_dns_server(dns_server);
 *     if (err) {
 *         printf("Failed to add DNS server: %s\n", err->message.c_str());
 *         return -1;
 *     }
 *     while (true) {
 *         sleep_ms(100);
 * 
 *         // poll here, because we do not call any pico_net functions that would poll internally in this example
 * #ifdef PICO_CYW43_ARCH_POLL
 *         cyw43_arch_poll();
 * #endif
 *     }
 *     return 0;
 * }
 * @endcode
 */
class DnsServer {
private:
    struct Ip4AddrHash {
        std::size_t operator()(const std::string& s) const noexcept {
            return std::hash<std::string>{}(s);
        }
    };

    std::unordered_map<std::string, ip4_addr_t, Ip4AddrHash> records;
    std::optional<ip4_addr_t> fallback_ip;

    udp_pcb* udp = nullptr;
    ip_addr_t listen_ip{};

    static constexpr uint16_t PORT_DNS_SERVER = 53;
    static constexpr size_t MAX_DNS_MSG_SIZE = 300;

    struct DnsHeader {
        uint16_t id;
        uint16_t flags;
        uint16_t question_count;
        uint16_t answer_record_count;
        uint16_t authority_record_count;
        uint16_t additional_record_count;
    };

    static void dns_recv_callback(void* arg, udp_pcb* upcb, pbuf* p, const ip_addr_t* src_addr, u16_t src_port);
    void handle_request(udp_pcb* upcb, pbuf* p, const ip_addr_t* src_addr, u16_t src_port);

    static std::string normalize_host(std::string host);
    static bool parse_ipv4(const std::string& ip, ip4_addr_t& out);
    bool extract_qname(const uint8_t* msg, size_t msg_len, std::string& out_host, const uint8_t*& question_end) const;

    std::optional<ip4_addr_t> lookup_host(const std::string& host) const;
    std::optional<Error> bind_socket(const ip_addr_t& bind_ip);
    std::optional<Error> send_response(const void* buf, size_t len, const ip_addr_t* dest, uint16_t port);
    
    friend class Network;

    [[nodiscard]] std::optional<Error> start(const ip_addr_t& bind_ip);
    [[nodiscard]] std::optional<Error> stop();

public:
    DnsServer() = default;
    ~DnsServer();

    /**
     * @brief Adds a DNS record mapping a hostname to an IP address.
     * @param host The hostname to map.
     * @param ip The IP address to map to the hostname.
     * @return An optional error if the operation fails.
     */
    std::optional<Error> add_record(const std::string& host, const std::string& ip);

    /**
     * @brief Removes a DNS record for the specified hostname.
     * @param host The hostname of the record to remove.
     * @return An optional error if the operation fails.
     */
    std::optional<Error> remove_record(const std::string& host);

    /**
     * @brief Clears all DNS records from the server.
     */
    void clear_records();

    /**
     * @brief Sets a fallback IP address to return for unresolved DNS queries.
     * @param ip The fallback IP address as a string.
     * @return An optional error if the operation fails.
     */
    std::optional<Error> set_fallback_ip(const std::string& ip);

    /**
     * @brief Clears the fallback IP address, causing unresolved DNS queries to fail instead of returning a default IP.
     */
    void clear_fallback_ip();
};