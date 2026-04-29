/**
 * @file udp_connection.cpp
 * @brief Implements the UdpConnection class for managing UDP connections using lwIP on the Raspberry Pi Pico W.
 * @author Felix Brodmann
 * @date 2026-04-27
 * @version 1.0.0
 */

#include "udp_connection.hpp"

void UdpConnection::attach_callbacks() {
    if (pcb == nullptr || !owns_pcb) {
        return;
    }

    cyw43_arch_lwip_begin();
    udp_recv(pcb, &UdpConnection::udp_recv_callback, this);
    cyw43_arch_lwip_end();
}

void UdpConnection::udp_recv_callback(void* arg, udp_pcb* pcb, pbuf* p,
                                      const ip_addr_t* addr, u16_t port) {
    if (arg == nullptr) {
        if (p != nullptr) {
            pbuf_free(p);
        }
        return;
    }

    static_cast<UdpConnection*>(arg)->handle_udp_recv(pcb, p, addr, port);
}

void UdpConnection::handle_udp_recv(udp_pcb* pcb, pbuf* p, const ip_addr_t* addr, u16_t port) {
    (void)pcb;

    if (p == nullptr) {
        return;
    }

    std::string ip = "";
    if (addr != nullptr && IP_GET_TYPE(addr) == IPADDR_TYPE_V4) {
        ip = ipaddr_ntoa(ip_2_ip4(addr));
    }

    Address from{"udp", ip, port};

    if (from.ip != remote_addr.ip || from.port != remote_addr.port) {
        pbuf_free(p);
        return;
    }

    Datagram d;
    d.from = std::move(from);
    d.data.resize(p->tot_len);

    pbuf_copy_partial(p, d.data.data(), p->tot_len, 0);
    pbuf_free(p);

    enqueue_datagram(std::move(d));
}

void UdpConnection::enqueue_datagram(Datagram datagram) {
    if (is_closed) {
        return;
    }

    rx_queue.push_back(std::move(datagram));
}

UdpConnection::~UdpConnection() {
    (void)close();
}

std::tuple<int, std::optional<Error>> UdpConnection::read(std::byte* buffer, size_t size) {
    if (buffer == nullptr) {
        return {0, Error("buffer is null")};
    }

    if (size == 0) {
        return {0, std::nullopt};
    }

    absolute_time_t until = nil_time;
    bool has_deadline = read_deadline_ms.has_value();
    if (has_deadline) {
        until = make_timeout_time_ms(*read_deadline_ms);
    }

    while (true) {
        auto [n, err] = read_nonblocking(buffer, size);
        if (err) {
            return {0, std::move(err)};
        }

        if (n > 0) {
            return {n, std::nullopt};
        }

        sleep_ms(10);

        if (has_deadline && absolute_time_diff_us(get_absolute_time(), until) <= 0) {
            return {0, Error("read timeout")};
        }
    }
}

std::tuple<int, std::optional<Error>> UdpConnection::read_nonblocking(std::byte* buffer, size_t size) {
    if (buffer == nullptr) {
        return {0, Error("buffer is null")};
    }

    if (size == 0) {
        return {0, std::nullopt};
    }

#if PICO_CYW43_ARCH_POLL
    cyw43_arch_poll();
#endif

    if (has_error) {
        return {0, Error(last_error_message)};
    }

    if (is_closed || pcb == nullptr) {
        return {0, Error("connection is closed")};
    }

    if (rx_queue.empty()) {
        return {0, std::nullopt};
    }

    Datagram d = std::move(rx_queue.front());
    rx_queue.erase(rx_queue.begin());

    size_t bytes_to_copy = std::min(size, d.data.size());
    std::memcpy(buffer, d.data.data(), bytes_to_copy);

    return {static_cast<int>(bytes_to_copy), std::nullopt};
}

std::tuple<int, std::optional<Error>> UdpConnection::write(const std::byte* buffer, size_t size) {
    if (pcb == nullptr || is_closed) {
        return {0, Error("connection is closed")};
    }

    if (buffer == nullptr) {
        return {0, Error("buffer is null")};
    }

    if (size == 0) {
        return {0, std::nullopt};
    }

    if (size > UINT16_MAX) {
        return {0, Error("udp packet too large")};
    }

    pbuf* p = pbuf_alloc(PBUF_TRANSPORT, static_cast<u16_t>(size), PBUF_RAM);
    if (p == nullptr) {
        return {0, Error("pbuf_alloc failed")};
    }

    std::memcpy(p->payload, buffer, size);

    err_t err = ERR_OK;

    cyw43_arch_lwip_begin();

    if (owns_pcb) {
        err = udp_send(pcb, p);
    } else {
        ip_addr_t remote_ip;
        bool is_valid_ipv4 = ip4addr_aton(remote_addr.ip.c_str(), ip_2_ip4(&remote_ip));

        if (!is_valid_ipv4) {
            cyw43_arch_lwip_end();
            pbuf_free(p);
            return {0, Error("Invalid IPv4 address format")};
        }

        IP_SET_TYPE_VAL(remote_ip, IPADDR_TYPE_V4);

        err = udp_sendto(pcb, p, &remote_ip, remote_addr.port);
    }

    cyw43_arch_lwip_end();

    pbuf_free(p);

    if (err != ERR_OK) {
        return {0, Error("udp_send failed")};
    }

    return {static_cast<int>(size), std::nullopt};
}

std::optional<Error> UdpConnection::close() {
    if (pcb == nullptr) {
        is_closed = true;
        rx_queue.clear();
        return std::nullopt;
    }

    cyw43_arch_lwip_begin();

    if (owns_pcb) {
        udp_recv(pcb, nullptr, nullptr);
        udp_remove(pcb);
    }

    cyw43_arch_lwip_end();

    if (!owns_pcb && listener_owner != nullptr) {
        listener_owner->remove_connection(remote_addr);
        listener_owner = nullptr;
    }

    pcb = nullptr;
    is_closed = true;
    rx_queue.clear();

    return std::nullopt;
}

Address UdpConnection::local_address() const {
    return local_addr;
}

Address UdpConnection::remote_address() const {
    return remote_addr;
}

std::optional<Error> UdpConnection::set_deadline(uint32_t timeout_ms) {
    read_deadline_ms = timeout_ms;
    write_deadline_ms = timeout_ms;
    return std::nullopt;
}

std::optional<Error> UdpConnection::set_read_deadline(uint32_t timeout_ms) {
    read_deadline_ms = timeout_ms;
    return std::nullopt;
}

std::optional<Error> UdpConnection::set_write_deadline(uint32_t timeout_ms) {
    write_deadline_ms = timeout_ms;
    return std::nullopt;
}