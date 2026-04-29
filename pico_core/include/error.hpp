#pragma once

#include <string>

/**
 * @struct Error
 * @brief Represents an error encapsulated as a string message.
 */
struct Error {
    /**
     * @brief The error message describing the error.
     */
    std::string message;

    ~Error() = default;
    explicit Error(std::string msg) : message(std::move(msg)) {}
};