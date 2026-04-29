/**
 * @file network.hpp
 * @brief Defines the Network class for managing network operations on the Pico device.
 * @author Felix Brodmann
 * @date 2026-04-27
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <tuple>
#include <optional>
#include <memory>
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"

#include "error.hpp"
#include "connection.hpp"
#include "tcp_connection.hpp"
#include "udp_connection.hpp"
#include "listener.hpp"
#include "tcp_listener.hpp"
#include "udp_listener.hpp"
#include "address.hpp"
#include "dhcp_server.hpp"
#include "dns_server.hpp"

/**
 * @class Network
 * @brief Represents the network interface for the Pico device.
 * @details This class provides methods for initializing the network, connecting to Wi-Fi, setting up an access point, managing DNS servers, and handling TCP and UDP connections. It uses the lwIP TCP/IP stack and the CYW43 Wi-Fi driver for network operations.
 * @par Example 1 (Access Point)
 * @code{.cpp}
 * #include <cstdio>
 * #include <memory>
 *
 * #include "network.hpp"
 * #include "listener.hpp"
 *
 * int main() {
 *     stdio_init_all();
 *
 *     Network network;
 *
 *     // Initialize network stack
 *     auto err = network.initialize();
 *     if (err) {
 *         printf("Network initialization failed: %s\n", err->message.c_str());
 *         return -1;
 *     }
 *
 *     // Start access point
 *     err = network.setup_wifi_access_point("PicoEcho", "12345678");
 *     if (err) {
 *         printf("AP setup failed: %s\n", err->message.c_str());
 *         return -1;
 *     }
 * 
 *     // go on from there with the rest of your application logic
 * 
 * }
 * @endcode
 * @par Example 2 (Wi-Fi Station Connection)
 * @code{.cpp}
 * #include <cstdio>
 * #include <memory>
 * 
 * #include "network.hpp"
 * #include "connection.hpp"
 * 
 * int main() {
 *     stdio_init_all();
 * 
 *     Network network;
 * 
 *     // Initialize network stack
 *     auto err = network.initialize();
 *     if (err) {
 *         printf("Network initialization failed: %s\n", err->message.c_str());
 *         return -1;
 *     }
 * 
 *     // Connect to Wi-Fi station
 *     err = network.connect_to_wifi_station("MyWiFi", "password123");
 *     if (err) {
 *         printf("Wi-Fi connection failed: %s\n", err->message.c_str());
 *         return -1;
 *     }
 * 
 *     // go on from there with the rest of your application logic
 * 
 * }
 * @endcode
 */
class Network {
private:
    bool is_initialized = false;
    bool is_connected_to_station_wifi = false;
    bool is_access_point_active = false;

    DhcpServer dhcp_server;

    DnsServer* attached_dns_server = nullptr;

    struct TcpDialState {
        bool done = false;
        bool success = false;
        err_t error = ERR_OK;
        tcp_pcb* connected_pcb = nullptr;
    };

    static err_t tcp_dial_connected_callback(void* arg, tcp_pcb* pcb, err_t err);

public:
    Network() = default;
    ~Network();
    
    /**
     * @brief Initializes the network interface.
     * @return An optional error if the operation fails.
     */
    [[nodiscard]] std::optional<Error> initialize();

    /**
     * @brief Connects to a Wi-Fi station.
     * @param ssid The SSID of the Wi-Fi network.
     * @param password The password of the Wi-Fi network.
     * @param timeout_ms The timeout in milliseconds.
     * @return An optional error if the operation fails.
     */
    [[nodiscard]] std::optional<Error> connect_to_wifi_station(const std::string& ssid, const std::string& password, uint32_t timeout_ms = 10000);

    /**
     * @brief Sets up a Wi-Fi access point.
     * @param ssid The SSID of the Wi-Fi network.
     * @param password The password of the Wi-Fi network.
     * @return An optional error if the operation fails.
     */
    [[nodiscard]] std::optional<Error> setup_wifi_access_point(const std::string& ssid, const std::string& password);

    /**
     * @brief Adds a DNS server.
     * @param dns_server The DNS server to add.
     * @return An optional error if the operation fails.
     */
    [[nodiscard]] std::optional<Error> add_dns_server(DnsServer& dns_server);

    /**
     * @brief Checks if the network is ready for use.
     * @return True if the network is ready, false otherwise.
     */
    bool is_ready() const { return is_connected_to_station_wifi || is_access_point_active; }

    /**
     * @brief Dials a TCP connection to the specified address.
     * @param address The address to connect to.
     * @param timeout_ms The timeout in milliseconds.
     * @return A tuple containing a unique pointer to the established connection and an optional error.
     */
    [[nodiscard]] std::tuple<std::unique_ptr<Connection>, std::optional<Error>> dial_tcp(const Address& address, uint32_t timeout_ms = 10000);

    /**
     * @brief Listens for incoming TCP connections on the specified port.
     * @param port The port to listen on.
     * @return A tuple containing a unique pointer to the TCP listener and an optional error.
     */
    [[nodiscard]] std::tuple<std::unique_ptr<Listener>, std::optional<Error>> listen_tcp(uint16_t port);

    /**
     * @brief Dials a UDP connection to the specified address.
     * @param address The address to connect to.
     * @return A tuple containing a unique pointer to the established connection and an optional error.
     */
    [[nodiscard]] std::tuple<std::unique_ptr<Connection>, std::optional<Error>> dial_udp(const Address& address);

    /**
     * @brief Listens for incoming UDP datagrams on the specified port.
     * @param port The port to listen on.
     * @return A tuple containing a unique pointer to the UDP listener and an optional error.
     */
    [[nodiscard]] std::tuple<std::unique_ptr<Listener>, std::optional<Error>> listen_udp(uint16_t port);
};