/**
 * @file dhcp_server.hpp
 * @brief Defines the DhcpServer class for managing a DHCP server on the Raspberry Pi Pico W.
 * @author Felix Brodmann
 * @date 2026-04-27
 * @version 1.0.0
 */

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <cstring>

#include "pico/cyw43_arch.h"
#include "cyw43_config.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/udp.h"

#include "error.hpp"


/**
 * @class DhcpServer
 * @brief A simple DHCP server implementation for the Raspberry Pi Pico W.
 * @details This class provides methods to start and stop a DHCP server that can assign IP addresses to clients on the local network. It listens for DHCP requests and responds with appropriate DHCP messages to manage IP address leases. The server maintains a pool of available IP addresses and tracks active leases based on client MAC addresses. It also handles DHCP options such as subnet mask, router, and DNS server information.
 */
class DhcpServer {
private:
    static constexpr uint8_t DHCPDISCOVER = 1;
    static constexpr uint8_t DHCPOFFER    = 2;
    static constexpr uint8_t DHCPREQUEST  = 3;
    static constexpr uint8_t DHCPACK      = 5;

    static constexpr uint8_t DHCP_OPT_SUBNET_MASK        = 1;
    static constexpr uint8_t DHCP_OPT_ROUTER             = 3;
    static constexpr uint8_t DHCP_OPT_DNS                = 6;
    static constexpr uint8_t DHCP_OPT_REQUESTED_IP       = 50;
    static constexpr uint8_t DHCP_OPT_IP_LEASE_TIME      = 51;
    static constexpr uint8_t DHCP_OPT_MSG_TYPE           = 53;
    static constexpr uint8_t DHCP_OPT_SERVER_ID          = 54;
    static constexpr uint8_t DHCP_OPT_END                = 255;

    static constexpr uint16_t PORT_DHCP_SERVER = 67;
    static constexpr uint16_t PORT_DHCP_CLIENT = 68;

    static constexpr uint32_t DEFAULT_LEASE_TIME_S = 24 * 60 * 60;
    static constexpr uint8_t MAC_LEN = 6;

    static constexpr uint8_t DHCPS_BASE_IP = 16;
    static constexpr uint8_t DHCPS_MAX_IP = 32;

    struct Lease {
        uint8_t mac[6]{};
        uint16_t expiry = 0;
    };

    struct DhcpMessage {
        uint8_t op;
        uint8_t htype;
        uint8_t hlen;
        uint8_t hops;
        uint32_t xid;
        uint16_t secs;
        uint16_t flags;
        uint8_t ciaddr[4];
        uint8_t yiaddr[4];
        uint8_t siaddr[4];
        uint8_t giaddr[4];
        uint8_t chaddr[16];
        uint8_t sname[64];
        uint8_t file[128];
        uint8_t options[312];
    };

    udp_pcb* udp = nullptr;
    ip_addr_t server_ip{};
    ip_addr_t netmask{};
    std::array<Lease, DHCPS_MAX_IP> leases{};

    static void udp_recv_callback(void* arg, udp_pcb* upcb, pbuf* p, const ip_addr_t* src_addr, u16_t src_port);
    void handle_request(udp_pcb* upcb, pbuf* p, const ip_addr_t* src_addr, u16_t src_port);

    static uint8_t* find_option(uint8_t* opt, uint8_t code);
    static void write_option_n(uint8_t** opt, uint8_t code, size_t n, const void* data);
    static void write_option_u8(uint8_t** opt, uint8_t code, uint8_t value);
    static void write_option_u32(uint8_t** opt, uint8_t code, uint32_t value);

    [[nodiscard]] std::optional<Error> send_to(netif* nif, const void* buffer, size_t len, uint32_t ip, uint16_t port);
    int find_or_allocate_lease(const uint8_t* mac);

public:
    DhcpServer() = default;
    ~DhcpServer();

    /**
     * @brief Starts the DHCP server with the specified IP address and netmask.
     * @param ip The IP address as a string to assign to the DHCP server.
     * @param netmask The subnet mask as a string for the DHCP server.
     * @return An optional error if the operation fails.
     */
    [[nodiscard]] std::optional<Error> start(const std::string& ip, const std::string& netmask);

    /**
     * @brief Starts the DHCP server with the specified IP address and netmask.
     * @param ip The IP address to assign to the DHCP server.
     * @param netmask The subnet mask for the DHCP server.
     * @return An optional error if the operation fails.
     */
    [[nodiscard]] std::optional<Error> start(const ip_addr_t& ip, const ip_addr_t& netmask);

    /**
     * @brief Stops the DHCP server.
     * @return An optional error if the operation fails.
     * @note This function should be called explictly to handle any potential errors during shutdown, as the destructor will not report errors.
     */
    [[nodiscard]] std::optional<Error> stop();

    /**
     * @brief Checks if the DHCP server is currently running.
     * @return True if the server is running, false otherwise.
     */
    bool is_running() const { return udp != nullptr; }
};