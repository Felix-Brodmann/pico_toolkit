/**
 * @file udp_listener.cpp
 * @brief Implements the UdpListener class for accepting UDP "connections" using lwIP on the Raspberry Pi Pico W.
 * @author Felix Brodmann
 * @date 2026-04-27
 * @version 1.0.0
 */

#include "udp_listener.hpp"

void UdpListener::attach_callbacks() {
    cyw43_arch_lwip_begin();
    udp_recv(pcb, &UdpListener::udp_recv_callback, this);
    cyw43_arch_lwip_end();
}

UdpListener::~UdpListener() {
    (void)close();
}

void UdpListener::udp_recv_callback(void* arg,udp_pcb* pcb,pbuf* p,const ip_addr_t* addr,u16_t port) {
    if (arg == nullptr) {
        if (p != nullptr) {
            pbuf_free(p);
        }
        return;
    }

    static_cast<UdpListener*>(arg)->handle_udp_recv(pcb, p, addr, port);
}

void UdpListener::handle_udp_recv(udp_pcb* pcb,pbuf* p,const ip_addr_t* addr,u16_t port) {
    (void)pcb;

    if (is_closed) {
        if (p != nullptr) {
            pbuf_free(p);
        }
        return;
    }

    if (p == nullptr) {
        return;
    }

    if (addr == nullptr || IP_GET_TYPE(addr) != IPADDR_TYPE_V4) {
        pbuf_free(p);
        return;
    }

    std::string remote_ip = ipaddr_ntoa(ip_2_ip4(addr));
    Address remote_addr{"udp", remote_ip, port};

    UdpConnection::Datagram datagram;
    datagram.from = remote_addr;
    datagram.data.resize(p->tot_len);

    pbuf_copy_partial(p, datagram.data.data(), p->tot_len, 0);
    pbuf_free(p);

    std::string key = make_peer_key(remote_addr);

    auto existing = active_connections.find(key);
    if (existing != active_connections.end() && existing->second != nullptr) {
        existing->second->enqueue_datagram(std::move(datagram));
        return;
    }

    auto conn = std::unique_ptr<UdpConnection>(
        new UdpConnection(
            this->pcb,
            local_addr,
            remote_addr,
            false,
            this
        )
    );

    conn->enqueue_datagram(std::move(datagram));

    active_connections[key] = conn.get();
    pending_connections.push_back(std::move(conn));
}

void UdpListener::remove_connection(const Address& remote_addr) {
    std::string key = make_peer_key(remote_addr);
    active_connections.erase(key);
}

std::tuple<std::unique_ptr<Connection>, std::optional<Error>> UdpListener::accept() {
    while (true) {
        auto [conn, err] = accept_nonblocking();

        if (err) {
            return {nullptr, std::move(err)};
        }

        if (conn != nullptr) {
            return {std::move(conn), std::nullopt};
        }

        sleep_ms(10);
    }
}

std::optional<Error> UdpListener::close() {
    if (pcb == nullptr) {
        is_closed = true;
        return std::nullopt;
    }

    cyw43_arch_lwip_begin();

    udp_recv(pcb, nullptr, nullptr);

    for (auto& conn : pending_connections) {
        if (conn != nullptr) {
            conn->close();
        }
    }

    pending_connections.clear();
    active_connections.clear();

    udp_remove(pcb);

    cyw43_arch_lwip_end();

    pcb = nullptr;
    is_closed = true;

    return std::nullopt;
}

Address UdpListener::address() const {
    return local_addr;
}

std::tuple<std::unique_ptr<Connection>, std::optional<Error>> UdpListener::accept_nonblocking() {
    if (is_closed) {
        return {nullptr, Error("listener is closed")};
    }

    if (has_error) {
        return {nullptr, Error(last_error_message)};
    }

#if PICO_CYW43_ARCH_POLL
    cyw43_arch_poll();
#endif

    if (pending_connections.empty()) {
        return {nullptr, std::nullopt};
    }

    std::unique_ptr<UdpConnection> conn = std::move(pending_connections.front());
    pending_connections.pop_front();

    return {std::move(conn), std::nullopt};
}

void UdpListener::handle_udp_error(const std::string& msg) {
    has_error = true;
    last_error_message = msg;
}

std::string UdpListener::make_peer_key(const Address& address) {
    return address.ip + ":" + std::to_string(address.port);
}