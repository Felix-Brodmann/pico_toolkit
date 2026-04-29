/**
 * @file tcp_connection.hpp
 * @brief Defines the TcpConnection class for managing TCP connections.
 * @author Felix Brodmann
 * @date 2026-04-27
 * @version 1.0.0
 */

#pragma once

#include <cstring>
#include <string>
#include <utility>
#include <tuple>
#include <optional>
#include <cstddef>
#include <vector>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "lwip/ip_addr.h"

#include "error.hpp"
#include "connection.hpp"
#include "address.hpp"

class Network;
class TcpListener;

/**
 * @class TcpConnection
 * @brief Represents a TCP connection.
 * @details This class encapsulates a TCP connection, providing methods for reading, writing, and managing the connection. It uses the lwIP TCP/IP stack for handling TCP connections.
 * @par Example
 * @code{.cpp}
 * #include <cstdio>
 * #include <cstring>
 *
 * #include "network.hpp"
 * #include "connection.hpp"
 *
 * int main() {
 *     stdio_init_all();
 *
 *     Network network;
 *
 *     // Initialize and configure your network here
 *
 *     Address addr{"tcp", "192.168.4.10", 12345};
 *
 *     auto [conn, err] = network.dial_tcp(addr);
 *     if (err) {
 *         printf("TCP dial failed: %s\n", err->message.c_str());
 *         return -1;
 *     }
 *
 *     // Send data
 *     const char* msg = "Hello, server!";
 *     auto [written, write_err] = conn->write(
 *         reinterpret_cast<const std::byte*>(msg),
 *         strlen(msg)
 *     );
 *
 *     if (write_err) {
 *         printf("TCP write failed: %s\n", write_err->message.c_str());
 *         return -1;
 *     }
 *
 *     // Receive data
 *     std::byte buffer[256];
 *     auto [n, read_err] = conn->read(buffer, sizeof(buffer) - 1);
 *
 *     if (read_err) {
 *         printf("TCP read failed: %s\n", read_err->message.c_str());
 *         return -1;
 *     }
 *     if (n == 0) {
 *         printf("Connection closed by peer\n");
 *     }
 * 
 *     buffer[n] = std::byte{0};
 *     printf("Received: %s\n", reinterpret_cast<char*>(buffer));
 *
 *     conn->close();
 *
 *     return 0;
 * }
 * @endcode
 */
class TcpConnection : public Connection {
private:
    tcp_pcb* pcb;
    Address local_addr;
    Address remote_addr;

    std::vector<std::byte> rx_buffer;
    bool is_closed = false;
    bool has_error = false;
    std::string last_error_message;

    std::optional<uint32_t> read_deadline_ms;
    std::optional<uint32_t> write_deadline_ms;

    TcpConnection(tcp_pcb* pcb, Address local_addr, Address remote_addr) : pcb(pcb), local_addr(std::move(local_addr)), remote_addr(std::move(remote_addr)) {}

    friend class Network;
    friend class TcpListener;

    void attach_callbacks();

    static err_t tcp_recv_callback(void* arg, tcp_pcb* pcb, pbuf* p, err_t err);
    static void tcp_err_callback(void* arg, err_t err);

    err_t handle_tcp_recv(tcp_pcb* pcb, pbuf* p, err_t err);
    void handle_tcp_err(err_t err);

public:
    ~TcpConnection();

    [[nodiscard]] std::tuple<int, std::optional<Error>> read(std::byte* buffer, size_t size) override;
    [[nodiscard]] std::tuple<int, std::optional<Error>> write(const std::byte* buffer, size_t size) override;
    [[nodiscard]] std::optional<Error> close() override;
    Address local_address() const override;
    Address remote_address() const override;
    [[nodiscard]] std::optional<Error> set_deadline(uint32_t timeout_ms) override;
    [[nodiscard]] std::optional<Error> set_read_deadline(uint32_t timeout_ms) override;
    [[nodiscard]] std::optional<Error> set_write_deadline(uint32_t timeout_ms) override;
    [[nodiscard]] std::tuple<int, std::optional<Error>> read_nonblocking(std::byte* buffer, size_t size) override;
};