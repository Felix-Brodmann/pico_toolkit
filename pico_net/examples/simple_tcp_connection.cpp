#include <cstdio>
#include <cstring>
#include <string>

#include "pico/stdlib.h"

#include "address.hpp"
#include "connection.hpp"
#include "network.hpp"

const std::string WIFI_SSID     = "YOUR_WIFI_SSID";
const std::string WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

int main()
{
    stdio_init_all();

    // time to open a serial monitor and see the output
    sleep_ms(5000);

    Network network;

    // initialize the network interface
    auto err = network.initialize();
    if (err)
    {
        printf("Network init failed: %s\n", err->message.c_str());
        return -1;
    }

    // connect to Wi-Fi station
    printf("Connecting to WiFi...\n");
    err = network.connect_to_wifi_station(WIFI_SSID, WIFI_PASSWORD);
    if (err)
    {
        printf("WiFi connection failed: %s\n", err->message.c_str());
        return -1;
    }
    printf("WiFi connected\n");

    // target address to connect to (for testing, you can set up a simple TCP server on your computer and use its local IP address and port here)
    // e.g. 'nc -l 12345' to start a TCP server on port 12345, and use your computer's local IP address (e.g. 192.168.1.17) in the addr below
    Address addr{"tcp", "192.168.1.17", 12345};

    // connect to the server
    printf("Dialing TCP %s:%d...\n", addr.ip.c_str(), addr.port);
    auto [conn, dial_err] = network.dial_tcp(addr);
    if (dial_err)
    {
        printf("TCP dial failed: %s\n", dial_err->message.c_str());
        return -1;
    }
    printf("TCP connection established\n");

    // send a message to the server
    const char *msg           = "Hello from Pico!\n";
    auto [written, write_err] = conn->write(reinterpret_cast<const std::byte *>(msg),  // cast the message to std::bytes
                                            strlen(msg));
    if (write_err)
    {
        printf("Write failed: %s\n", write_err->message.c_str());
        return -1;
    }
    printf("Sent: %s", msg);

    // read a response from the server
    std::byte buffer[256];
    auto [n, read_err] = conn->read(buffer, sizeof(buffer) - 1);
    if (read_err)
    {
        printf("Read failed: %s\n", read_err->message.c_str());
        return -1;
    }

    // check if the connection was closed by the peer
    if (n > 0)
    {
        // null-terminate the received data and print it
        buffer[n] = std::byte{0};
        printf("Received: %s\n", reinterpret_cast<char *>(buffer));
    }
    else
    {
        printf("Connection closed by peer\n");
    }

    // close the connection cleanly
    conn->close();
    printf("Done\n");

    return 0;
}