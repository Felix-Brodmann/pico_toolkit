# pico_net

`pico_net` is a C++ networking library for the Raspberry Pi Pico W.
It wraps `cyw43` + `lwIP` in a simpler API for common tasks:

- initialize Wi-Fi
- connect as station (STA)
- start an access point (AP)
- establish TCP and UDP connections
- run a TCP listener
- optionally provide a small DNS server

The library is designed for embedded workloads and uses simple error handling via `std::optional<Error>`.

## Features

- Unified C++ API via `Network`, `Connection`, and `Listener`
- Blocking and non-blocking reads
- Read/write deadlines on connections
- TCP Client (`dial_tcp`) und TCP Server (`listen_tcp`)
- UDP Client (`dial_udp`)
- Integrated DNS server with static records + fallback IP
- AP setup including DHCP server

## Requirements

- Raspberry Pi Pico SDK (including `cyw43`/`lwIP`)
- CMake project with a Pico W target board (for example `PICO_BOARD pico_w`)
- `pico_core` must be added before `pico_net`

## CMake Integration

In your root `CMakeLists.txt`:

```cmake
add_subdirectory(lib/pico_core)
add_subdirectory(lib/pico_net)

target_link_libraries(${PROJECT_NAME}
    pico_net
    pico_stdlib
)
```

Note: `pico_net` checks whether the `pico_core` target exists.

## Public API (Quick Overview)

- `Network::initialize()`
- `Network::connect_to_wifi_station(ssid, password, timeout_ms)`
- `Network::setup_wifi_access_point(ssid, password)`
- `Network::add_dns_server(dns_server)`
- `Network::dial_tcp(address, timeout_ms)`
- `Network::listen_tcp(port)`
- `Network::dial_udp(address)`

Address format:

```cpp
Address{"tcp" | "udp", "a.b.c.d", port}
```

Connections are exposed through the `Connection` interface:

- `read(...)`, `read_nonblocking(...)`
- `write(...)`
- `close()`
- `set_deadline(...)`, `set_read_deadline(...)`, `set_write_deadline(...)`

## Example 1: Wi-Fi Station + TCP Dial

```cpp
#include <cstdio>
#include <cstring>
#include "network.hpp"

int main() {
    stdio_init_all();

    Network net;
    if (auto err = net.initialize()) {
        printf("init failed: %s\n", err->message.c_str());
        return -1;
    }

    if (auto err = net.connect_to_wifi_station("MY_SSID", "MY_PASS", 10000)) {
        printf("wifi failed: %s\n", err->message.c_str());
        return -1;
    }

    Address server{"tcp", "192.168.1.100", 1883};
    auto [conn, dial_err] = net.dial_tcp(server, 5000);
    if (dial_err) {
        printf("tcp dial failed: %s\n", dial_err->message.c_str());
        return -1;
    }

    const char* msg = "hello\n";
    auto [written, write_err] = conn->write(reinterpret_cast<const std::byte*>(msg), std::strlen(msg));
    if (write_err) {
        printf("write failed: %s\n", write_err->message.c_str());
        return -1;
    }

    printf("sent %d bytes\n", written);
    return 0;
}
```

## Example 2: AP + DNS Server

```cpp
#include <cstdio>
#include "network.hpp"

int main() {
    stdio_init_all();

    Network net;
    if (auto err = net.initialize()) {
        printf("init failed: %s\n", err->message.c_str());
        return -1;
    }

    if (auto err = net.setup_wifi_access_point("PicoAP", "12345678")) {
        printf("ap failed: %s\n", err->message.c_str());
        return -1;
    }

    DnsServer dns;
    dns.add_record("pico.local", "192.168.4.1");
    dns.set_fallback_ip("192.168.4.1");

    if (auto err = net.add_dns_server(dns)) {
        printf("dns failed: %s\n", err->message.c_str());
        return -1;
    }

    while (true) {
        sleep_ms(10);
    }
}
```

## Example 3: TCP Server (Echo)

```cpp
#include <vector>
#include "network.hpp"

int main() {
    stdio_init_all();

    Network net;
    if (auto err = net.initialize()) return -1;
    if (auto err = net.setup_wifi_access_point("PicoEcho", "12345678")) return -1;

    auto [listener, listen_err] = net.listen_tcp(19055);
    if (listen_err) return -1;

    std::vector<std::unique_ptr<Connection>> clients;
    std::byte buf[256];

    while (true) {
        auto [new_conn, accept_err] = listener->accept_nonblocking();
        if (accept_err) break;
        if (new_conn) clients.push_back(std::move(new_conn));

        for (auto it = clients.begin(); it != clients.end();) {
            auto [n, read_err] = (*it)->read_nonblocking(buf, sizeof(buf));
            if (read_err) {
                (*it)->close();
                it = clients.erase(it);
                continue;
            }

            if (n > 0) {
                auto [wn, write_err] = (*it)->write(buf, n);
                (void)wn;
                if (write_err) {
                    (*it)->close();
                    it = clients.erase(it);
                    continue;
                }
            }

            ++it;
        }

        sleep_ms(1);
    }

    return 0;
}
```

## Behavior and Design Notes

- The API currently targets IPv4.
- `read_nonblocking(...)` returns `{0, nullopt}` when no data is currently available.
- Connection failures return an `Error` with a message.
- AP and STA are intended as mutually exclusive modes (not used at the same time).
- In all network operations, the corosponding functions alreadt call `cyw43_arch_poll()` internally, so no additional calls are needed for basic use cases.

## Project Structure (pico_net)

- `include/` Public headers
- `src/` C++ implementation
- `examples/` Example applications
