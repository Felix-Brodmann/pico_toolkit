/**
 * @file udp_listener.hpp
 * @brief Defines the UdpListener class for accepting logical UDP peer connections.
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
#include <unordered_map>
#include <cstddef>
#include <algorithm>
#include <cstring>

#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"
#include "pico/cyw43_arch.h"
#include "pico/time.h"

#include "listener.hpp"
#include "connection.hpp"
#include "udp_connection.hpp"
#include "address.hpp"
#include "error.hpp"

class Network;
class UdpConnection;

/**
 * @class UdpListener
 * @brief Represents a UDP listener that accepts logical UDP peer connections.
 * @details This class binds a UDP PCB to a local port and creates one logical UdpConnection per remote endpoint. A remote endpoint is identified by remote IP address and remote port.
 */
class UdpListener : public Listener {
private:
    udp_pcb* pcb = nullptr;
    Address local_addr;

    bool is_closed = false;
    bool has_error = false;
    std::string last_error_message;

    std::deque<std::unique_ptr<UdpConnection>> pending_connections;

    std::unordered_map<std::string, UdpConnection*> active_connections;

    static void udp_recv_callback(void* arg, udp_pcb* pcb, pbuf* p, const ip_addr_t* addr, u16_t port);
    void handle_udp_recv(udp_pcb* pcb,pbuf* p,const ip_addr_t* addr,u16_t port);
    void handle_udp_error(const std::string& msg);

    static std::string make_peer_key(const Address& address);

    UdpListener(udp_pcb* pcb, Address local_addr) : pcb(pcb), local_addr(std::move(local_addr)) {}

    friend class Network;
    friend class UdpConnection;

    void attach_callbacks() override;

    void remove_connection(const Address& remote_addr);

public:
    ~UdpListener() override;

    [[nodiscard]] std::tuple<std::unique_ptr<Connection>, std::optional<Error>> accept() override;
    [[nodiscard]] std::optional<Error> close() override;
    Address address() const override;

    [[nodiscard]] std::tuple<std::unique_ptr<Connection>, std::optional<Error>> accept_nonblocking() override;
};