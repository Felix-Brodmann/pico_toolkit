/**
 * @file tcp_listener.hpp
 * @brief Defines the TcpListener class for accepting incoming TCP connections.
 * @author Felix Brodmann
 * @date 2026-04-27
 * @version 1.0.0
 */

#pragma once

#include <deque>
#include <memory>
#include <optional>
#include <tuple>
#include <string>
#include "lwip/tcp.h"

#include "listener.hpp"
#include "connection.hpp"
#include "tcp_connection.hpp"
#include "address.hpp"
#include "error.hpp"

class Network;

/**
 * @class TcpListener
 * @brief Represents a TCP listener that accepts incoming TCP connections.
 * @details This class encapsulates a TCP listener, providing methods for accepting incoming connections and managing the listener. It uses the lwIP TCP/IP stack for handling TCP connections.
 * @par Example
 * @code{.cpp}
 * #include <cstdio>
 * #include <cstring>
 * #include <memory>
 * #include <vector>
 *
 * #include "network.hpp"
 * #include "connection.hpp"
 *
 * int main() {
 *     stdio_init_all();
 *
 *     Network network;
 *
 *     // Initialize and configure your network here.
 *     // For example: station mode, access point mode, DHCP, DNS, etc.
 *
 *     auto [listener, listen_err] = network.listen_tcp(12345);
 *     if (listen_err) {
 *         printf("TCP listen failed: %s\n", listen_err->message.c_str());
 *         return -1;
 *     }
 *
 *     printf("TCP server listening on %s:%d\n",
 *         listener->address().ip.c_str(),
 *         listener->address().port);
 *
 *     std::vector<std::unique_ptr<Connection>> clients;
 *
 *     while (true) {
 *         auto [new_conn, accept_err] = listener->accept_nonblocking();
 *         if (accept_err) {
 *             printf("Accept failed: %s\n", accept_err->message.c_str());
 *             break;
 *         }
 *
 *         if (new_conn) {
 *             printf("Accepted TCP client %s:%d\n",
 *                 new_conn->remote_address().ip.c_str(),
 *                 new_conn->remote_address().port);
 *
 *             clients.push_back(std::move(new_conn));
 *         }
 *
 *         // Iterate over clients
 *
 *         for (auto it = clients.begin(); it != clients.end(); ) {
 *             auto& client = *it;
 *
 *             // Handle client communication here, e.g. read/write data, check for disconnection, etc.
 *
 *             ++it;
 *         }
 *
 *         sleep_ms(1);
 *     }
 *
 *     return 0;
 * }
 * @endcode
 */
class TcpListener : public Listener {
private:
    tcp_pcb* pcb = nullptr;
    Address local_addr;

    bool is_closed = false;
    bool has_error = false;
    std::string last_error_message;

    std::deque<std::unique_ptr<TcpConnection>> pending_connections;

    static err_t tcp_accept_callback(void* arg, tcp_pcb* newpcb, err_t err);
    err_t handle_tcp_accept(tcp_pcb* newpcb, err_t err);
    void handle_tcp_error(const std::string& msg);

    TcpListener(tcp_pcb* pcb, Address local_addr) : pcb(pcb), local_addr(std::move(local_addr)) {}

    friend class Network;

    void attach_callbacks() override;

public:
    ~TcpListener() override;

    [[nodiscard]] std::tuple<std::unique_ptr<Connection>, std::optional<Error>> accept() override;
    [[nodiscard]] std::optional<Error> close() override;
    Address address() const override;

    [[nodiscard]] std::tuple<std::unique_ptr<Connection>, std::optional<Error>> accept_nonblocking() override;
};