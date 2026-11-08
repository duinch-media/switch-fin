#pragma once

#include <string>
#include <variant>
#include <optional>

namespace domain {

enum class ErrorCode {
    OK = 0,
    NETWORK_ERROR,
    UNAUTHORIZED,
    NOT_FOUND,
    INVALID_PAYLOAD,
    PLAYBACK_FAILED,
    STORAGE_ERROR,
    CANCELLED,
    UNKNOWN_ERROR,
};

struct DomainError {
    ErrorCode code = ErrorCode::UNKNOWN_ERROR;
    std::string message;

    static DomainError network(std::string msg) {
        return {ErrorCode::NETWORK_ERROR, std::move(msg)};
    }

    static DomainError unauthorized(std::string msg = "Unauthorized access") {
        return {ErrorCode::UNAUTHORIZED, std::move(msg)};
    }

    static DomainError notFound(std::string msg = "Resource not found") {
        return {ErrorCode::NOT_FOUND, std::move(msg)};
    }

    static DomainError playback(std::string msg) {
        return {ErrorCode::PLAYBACK_FAILED, std::move(msg)};
    }

    static DomainError cancelled() {
        return {ErrorCode::CANCELLED, "Operation cancelled"};
    }
};

template <typename T>
class Result {
public:
    Result(T value) : m_data(std::move(value)) {}
    Result(DomainError error) : m_data(std::move(error)) {}

    bool isOk() const noexcept {
        return std::holds_alternative<T>(m_data);
    }

    bool isError() const noexcept {
        return std::holds_alternative<DomainError>(m_data);
    }

    const T& value() const {
        return std::get<T>(m_data);
    }

    T& value() {
        return std::get<T>(m_data);
    }

    const DomainError& error() const {
        return std::get<DomainError>(m_data);
    }

    std::optional<T> ok() const {
        if (isOk()) return std::get<T>(m_data);
        return std::nullopt;
    }

private:
    std::variant<T, DomainError> m_data;
};

} // namespace domain
