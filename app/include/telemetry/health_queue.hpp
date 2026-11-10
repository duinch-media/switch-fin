#pragma once

#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace telemetry {

/// File-backed FIFO of NDJSON lines (one app-health event per line).
/// Survives restarts; the oldest lines are dropped when a cap is exceeded.
class HealthQueue {
public:
    explicit HealthQueue(std::string path, size_t maxBytes = 256 * 1024, size_t maxLines = 500);

    /// Append one JSON object (no newline). Enforces the caps.
    void append(const std::string& line);
    /// Oldest-first copy of up to n lines.
    std::vector<std::string> peek(size_t n) const;
    /// Remove the oldest n lines.
    void drop(size_t n);
    size_t size() const;
    /// Lines discarded by the caps since construction.
    size_t overflowDropped() const;

private:
    std::vector<std::string> readAll() const;
    void writeAll(const std::vector<std::string>& lines) const;

    std::string path;
    size_t maxBytes;
    size_t maxLines;
    size_t overflow = 0;
    mutable std::mutex mutex;
};

}  // namespace telemetry
