/**
 * @file tcp_connection.cpp
 * @brief Implements the TcpConnection class for managing TCP connections using lwIP on the Raspberry Pi Pico W.
 * @author Felix Brodmann
 * @date 2026-04-27
 * @version 1.0.0
 */

#include "tcp_connection.hpp"

void TcpConnection::attach_callbacks() {
    cyw43_arch_lwip_begin();
    tcp_arg(pcb, this);
    tcp_recv(pcb, &TcpConnection::tcp_recv_callback);
    tcp_err(pcb, &TcpConnection::tcp_err_callback);
    cyw43_arch_lwip_end();
}

err_t TcpConnection::tcp_recv_callback(void* arg, tcp_pcb* pcb, pbuf* p, err_t err) {
    if (arg == nullptr) {
        if (p != nullptr) {
            pbuf_free(p);
        }
        return ERR_OK;
    }

    return static_cast<TcpConnection*>(arg)->handle_tcp_recv(pcb, p, err);
}

err_t TcpConnection::handle_tcp_recv(tcp_pcb* pcb, pbuf* p, err_t err) {
    if (p == nullptr) {
        is_closed = true;
        this->pcb = nullptr;
        return ERR_OK;
    }

    if (err != ERR_OK) {
        pbuf_free(p);
        has_error = true;
        last_error_message = "tcp receive error";
        return err;
    }

    size_t old_size = rx_buffer.size();
    rx_buffer.resize(old_size + p->tot_len);

    pbuf_copy_partial(p, rx_buffer.data() + old_size, p->tot_len, 0);
    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);

    return ERR_OK;
}

void TcpConnection::tcp_err_callback(void* arg, err_t err) {
    if (arg == nullptr) {
        return;
    }

    static_cast<TcpConnection*>(arg)->handle_tcp_err(err);
}

void TcpConnection::handle_tcp_err(err_t err) {
    pcb = nullptr;
    has_error = true;
    is_closed = true;

    switch (err) {
        case ERR_ABRT: last_error_message = "tcp aborted"; break;
        case ERR_RST:  last_error_message = "tcp reset"; break;
        case ERR_CLSD: last_error_message = "tcp closed"; break;
        case ERR_CONN: last_error_message = "tcp connection error"; break;
        case ERR_TIMEOUT: last_error_message = "tcp timeout"; break;
        default: last_error_message = "tcp error"; break;
    }
}

TcpConnection::~TcpConnection() {
    auto err = close();
    if (err) {
        tcp_abort(pcb);
    }
}

std::tuple<int, std::optional<Error>> TcpConnection::read(std::byte* buffer, size_t size) {
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

std::tuple<int, std::optional<Error>> TcpConnection::write(const std::byte* buffer, size_t size) {
    if (pcb == nullptr || is_closed) {
        return {0, Error("connection is closed")};
    }

    if (size == 0) {
        return {0, std::nullopt};
    }

    cyw43_arch_lwip_begin();
    err_t err = tcp_write(pcb, buffer, size, TCP_WRITE_FLAG_COPY);
    if (err == ERR_OK) {
        err = tcp_output(pcb);
    }
    cyw43_arch_lwip_end();

    if (err != ERR_OK) {
        return {0, Error("tcp_write/tcp_output failed")};
    }

    return {static_cast<int>(size), std::nullopt};
}

std::optional<Error> TcpConnection::close() {
    if (pcb == nullptr) {
        is_closed = true;
        return std::nullopt;
    }

    cyw43_arch_lwip_begin();
    tcp_arg(pcb, nullptr);
    tcp_recv(pcb, nullptr);
    tcp_sent(pcb, nullptr);
    tcp_err(pcb, nullptr);

    err_t err = tcp_close(pcb);
    cyw43_arch_lwip_end();

    pcb = nullptr;
    is_closed = true;

    if (err != ERR_OK) {
        return Error("tcp_close failed");
    }

    return std::nullopt;
}

Address TcpConnection::local_address() const {
    return local_addr;
}

Address TcpConnection::remote_address() const {
    return remote_addr;
}

std::optional<Error> TcpConnection::set_deadline(uint32_t timeout_ms) {
    read_deadline_ms = timeout_ms;
    write_deadline_ms = timeout_ms;
    return std::nullopt;
}

std::optional<Error> TcpConnection::set_read_deadline(uint32_t timeout_ms) {
    read_deadline_ms = timeout_ms;
    return std::nullopt;
}

std::optional<Error> TcpConnection::set_write_deadline(uint32_t timeout_ms) {
    write_deadline_ms = timeout_ms;
    return std::nullopt;
}

std::tuple<int, std::optional<Error>> TcpConnection::read_nonblocking(std::byte* buffer, size_t size) {
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

    if (rx_buffer.empty()) {
        if (is_closed) {
            return {0, Error("connection closed by peer")};
        }

        if (pcb == nullptr) {
            return {0, Error("connection is closed")};
        }

        return {0, std::nullopt};
    }

    size_t bytes_to_copy = std::min(size, rx_buffer.size());
    std::memcpy(buffer, rx_buffer.data(), bytes_to_copy);
    rx_buffer.erase(rx_buffer.begin(), rx_buffer.begin() + bytes_to_copy);

    return {static_cast<int>(bytes_to_copy), std::nullopt};
}