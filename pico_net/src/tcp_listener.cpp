/**
 * @file tcp_listener.cpp
 * @brief Implements the TcpListener class for accepting TCP connections using lwIP on the Raspberry Pi Pioco W.
 * @author Felix Brodmann
 * @date 2026-04-27
 * @version 1.0.0
 */

#include "tcp_listener.hpp"

TcpListener::~TcpListener()
{
    auto err = close();
    if (err) {
        tcp_abort(pcb);
    }
}

void TcpListener::attach_callbacks()
{
    cyw43_arch_lwip_begin();
    tcp_arg(pcb, this);
    tcp_accept(pcb, &TcpListener::tcp_accept_callback);
    cyw43_arch_lwip_end();
}

err_t TcpListener::tcp_accept_callback(void* arg, tcp_pcb* newpcb, err_t err)
{
    if (arg == nullptr) {
        if (newpcb != nullptr) {
            tcp_abort(newpcb);
        }
        return ERR_ABRT;
    }

    return static_cast<TcpListener*>(arg)->handle_tcp_accept(newpcb, err);
}

err_t TcpListener::handle_tcp_accept(tcp_pcb* newpcb, err_t err)
{
    cyw43_arch_lwip_check();

    if (is_closed) {
        if (newpcb != nullptr) {
            tcp_abort(newpcb);
        }
        return ERR_ABRT;
    }

    if (err != ERR_OK || newpcb == nullptr) {
        has_error = true;
        last_error_message = "tcp accept failed";
        return (err != ERR_OK) ? err : ERR_ABRT;
    }

    std::string remote_ip = "";
    uint16_t remote_port = newpcb->remote_port;
    uint16_t local_port = newpcb->local_port;

    if (IP_GET_TYPE(&newpcb->remote_ip) == IPADDR_TYPE_V4) {
        remote_ip = ipaddr_ntoa(ip_2_ip4(&newpcb->remote_ip));
    }

    Address accepted_local_addr{"tcp", local_addr.ip, local_port};
    Address remote_addr{"tcp", remote_ip, remote_port};

    auto conn = std::unique_ptr<TcpConnection>(
        new TcpConnection(newpcb, std::move(accepted_local_addr), std::move(remote_addr))
    );

    pending_connections.push_back(std::move(conn));

    return ERR_OK;
}

std::tuple<std::unique_ptr<Connection>, std::optional<Error>> TcpListener::accept() {
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

std::optional<Error> TcpListener::close()
{
    if (pcb == nullptr) {
        is_closed = true;
        return std::nullopt;
    }

    cyw43_arch_lwip_begin();

    for (auto& conn : pending_connections) {
        if (conn != nullptr) {
            conn->close();
        }
    }
    pending_connections.clear();

    tcp_arg(pcb, nullptr);
    tcp_accept(pcb, nullptr);

    err_t err = tcp_close(pcb);
    if (err != ERR_OK) {
        tcp_abort(pcb);
    }
    cyw43_arch_lwip_end();

    pcb = nullptr;
    is_closed = true;

    if (err != ERR_OK) {
        return Error("tcp_close failed");
    }

    return std::nullopt;
}

Address TcpListener::address() const
{
    return local_addr;
}

std::tuple<std::unique_ptr<Connection>, std::optional<Error>> TcpListener::accept_nonblocking()
{
    if (is_closed)
    {
        return {nullptr, Error("listener is closed")};
    }
    if (has_error)
    {
        return {nullptr, Error(last_error_message)};
    }

    #if PICO_CYW43_ARCH_POLL
        cyw43_arch_poll();
    #endif

    if (pending_connections.empty())
    {
        return {nullptr, std::nullopt};
    }

    std::unique_ptr<TcpConnection> conn = std::move(pending_connections.front());
    pending_connections.pop_front();

    conn->attach_callbacks();

    return {std::move(conn), std::nullopt};
}