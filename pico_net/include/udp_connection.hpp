/**
 * @file udp_connection.hpp
 * @brief Defines the UdpConnection class for managing UDP connections.
 * @author Felix Brodmann
 * @date 2026-04-27
 * @version 1.0.0
 */

#pragma once

#include <vector>
#include <tuple>
#include <optional>
#include <cstddef>
#include <algorithm>
#include <cstring>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/udp.h"
#include "lwip/ip_addr.h"

#include "error.hpp"
#include "connection.hpp"
#include "address.hpp"
#include "udp_listener.hpp"

class Network;
class UdpListener;

/**
 * @class UdpConnection
 * @brief Represents a UDP connection.
 * @details This class encapsulates a UDP connection, providing methods for reading, writing, and managing the connection. It uses the lwIP TCP/IP stack for handling UDP connections. Since UDP is connectionless, this class manages the remote address for sending and receiving datagrams, and it can be associated with a UdpListener for receiving datagrams from multiple remote endpoints.
 */
class UdpConnection : public Connection {
public:

    /**
     * @struct Datagram
     * @brief Represents a UDP datagram, containing the data and the source address.
     */
    struct Datagram {
        /**
         * @brief The data contained in the datagram.
         */
        std::vector<std::byte> data;

        /**
         * @brief The source address from which the datagram was received.
         */
        Address from;
    };

private:
    udp_pcb* pcb = nullptr;
    Address local_addr;
    Address remote_addr;

    bool owns_pcb = true;
    UdpListener* listener_owner = nullptr;

    std::vector<Datagram> rx_queue;
    bool is_closed = false;
    bool has_error = false;
    std::string last_error_message;

    std::optional<uint32_t> read_deadline_ms;
    std::optional<uint32_t> write_deadline_ms;

    UdpConnection(udp_pcb* pcb, Address local_addr, Address remote_addr, bool owns_pcb = true, UdpListener* listener_owner = nullptr) : pcb(pcb), local_addr(std::move(local_addr)), remote_addr(std::move(remote_addr)), owns_pcb(owns_pcb), listener_owner(listener_owner) {}

    friend class Network;
    friend class UdpListener;

    void attach_callbacks();

    static void udp_recv_callback(void* arg, udp_pcb* pcb, pbuf* p, const ip_addr_t* addr, u16_t port);
    void handle_udp_recv(udp_pcb* pcb, pbuf* p, const ip_addr_t* addr, u16_t port);

    void enqueue_datagram(Datagram datagram);
 
public:
    ~UdpConnection();

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