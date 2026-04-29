/**
 * @file address.hpp
 * @brief Defines the Address struct for representing network addresses.
 * @author Felix Brodmann
 * @date 2026-04-27
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <cstdint>
#include <utility>

/**
 * @struct Address
 * @brief Represents a network address, including the network type, IP address, and port number.
 * @details The `Address` struct encapsulates the necessary information to identify a network endpoint
 */
struct Address {
    /**
     * @brief The type of network (e.g., "tcp", "udp").
     */
    std::string network;
    /**
     * @brief The IPv4 address of the endpoint.
     */
    std::string ip;
    /**
     * @brief The port number associated with the endpoint.
     * @note This member is initialized to 0 by default, indicating that it is not set. It should be assigned a valid port number before use.
     */
    uint16_t port = 0;

    Address() = default;

    Address(std::string network, std::string ip, uint16_t port)
        : network(std::move(network)), ip(std::move(ip)), port(port) {}
};