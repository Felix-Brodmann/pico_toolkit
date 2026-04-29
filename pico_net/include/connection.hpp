/**
 * @file connection.hpp
 * @brief Defines the Connection class for representing network connections.
 * @author Felix Brodmann
 * @date 2026-04-27
 * @version 1.0.0
 */

#pragma once

#include <tuple>
#include <optional>
#include <cstddef>
#include <cstdint>

#include "error.hpp"
#include "address.hpp"

/**
 * @class Connection
 * @brief Represents a network connection between a local and remote endpoint.
 * @details This is an abstract base class that defines the interface for network connections. It provides methods for reading and writing data, closing the connection, retrieving local and remote addresses, and setting deadlines for read and write operations.
 */
class Connection {
public:
    virtual ~Connection() = default;

    /**
     * @brief Reads data from the connection.
     * @param buffer The buffer to read data into.
     * @param size The size of the buffer.
     * @return A tuple containing the number of bytes read and an optional error.
     */
    virtual std::tuple<int, std::optional<Error>> read(std::byte* buffer, size_t size) = 0;

    /**
     * @brief Writes data to the connection.
     * @param buffer The buffer containing the data to write.
     * @param size The size of the data to write.
     * @return A tuple containing the number of bytes written and an optional error.
     */
    virtual std::tuple<int, std::optional<Error>> write(const std::byte* buffer, size_t size) = 0;

    /**
     * @brief Closes the connection.
     * @return An optional error if the operation fails.
     */
    virtual std::optional<Error> close() = 0;

    /**
     * @brief Gets the local address of the connection.
     * @return The local address.
     */
    virtual Address local_address() const = 0;

    /**
     * @brief Gets the remote address of the connection.
     * @return The remote address.
     */
    virtual Address remote_address() const = 0;

    /**
     * @brief Sets the deadline for read and write operations on the connection.
     * @param timeout_ms The timeout in milliseconds.
     * @return An optional error if the operation fails.
     */
    virtual std::optional<Error> set_deadline(uint32_t timeout_ms) = 0;

    /**
     * @brief Sets the read deadline for the connection.
     * @param timeout_ms The timeout in milliseconds.
     * @return An optional error if the operation fails.
     */
    virtual std::optional<Error> set_read_deadline(uint32_t timeout_ms) = 0;

    /**
     * @brief Sets the write deadline for the connection.
     * @param timeout_ms The timeout in milliseconds.
     * @return An optional error if the operation fails.
     */
    virtual std::optional<Error> set_write_deadline(uint32_t timeout_ms) = 0;

    /**
     * @brief Reads data from the connection in a non-blocking manner.
     * @param buffer The buffer to read data into.
     * @param size The size of the buffer.
     * @return A tuple containing the number of bytes read and an optional error.
     */
    virtual std::tuple<int, std::optional<Error>> read_nonblocking(std::byte* buffer, size_t size) = 0;
};