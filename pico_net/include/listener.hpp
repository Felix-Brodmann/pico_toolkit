/**
 * @file listener.hpp
 * @brief Defines the Listener class for accepting incoming network connections.
 * @author Felix Brodmann
 * @date 2026-04-27
 * @version 1.0.0
 */

#pragma once

#include <memory>
#include <tuple>
#include <optional>

#include "error.hpp"
#include "address.hpp"
#include "connection.hpp"

/**
 * @class Listener
 * @brief Represents a network listener that accepts incoming connections.
 * @details This is an abstract base class that defines the interface for network listeners. It provides methods for accepting incoming connections, closing the listener, and retrieving the local address.
 */
class Listener {
private:
    virtual void attach_callbacks() = 0;

public:
    virtual ~Listener() = default;

    /**
     * @brief Accepts an incoming connection.
     * @return A tuple containing a unique pointer to the accepted connection and an optional error.
     */
    virtual std::tuple<std::unique_ptr<Connection>, std::optional<Error>> accept() = 0;

    /**
     * @brief Closes the listener.
     * @return An optional error if the operation fails.
     */
    virtual std::optional<Error> close() = 0;

    /**
     * @brief Gets the local address of the listener.
     * @return The local address.
     */
    virtual Address address() const = 0;

    /**
     * @brief Accepts an incoming connection in a non-blocking manner.
     * @return A tuple containing a unique pointer to the accepted connection and an optional error.
     */
    virtual std::tuple<std::unique_ptr<Connection>, std::optional<Error>> accept_nonblocking() = 0;
};