#include "telemetry/health_queue.hpp"

#include <cstdio>
#include <fstream>

namespace telemetry {

HealthQueue::HealthQueue(std::string path, size_t maxBytes, size_t maxLines)
    : path(std::move(path)), maxBytes(maxBytes), maxLines(maxLines) {}

std::vector<std::string> HealthQueue::readAll() const {
    std::vector<std::string> lines;
    std::ifstream in(this->path);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) lines.push_back(line);
    }
    return lines;
}

void HealthQueue::writeAll(const std::vector<std::string>& lines) const {
    // Write a temp file then swap it in, so a crash never leaves a half-written queue.
    // (rename() will not replace an existing file on every libnx filesystem, hence remove first.)
    const std::string tmp = this->path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        for (const auto& l : lines) out << l << '\n';
        if (!out) return;
    }
    std::remove(this->path.c_str());
    if (std::rename(tmp.c_str(), this->path.c_str()) != 0) std::remove(tmp.c_str());
}

void HealthQueue::append(const std::string& line) {
    if (line.empty() || line.find('\n') != std::string::npos) return;
    std::lock_guard<std::mutex> lock(this->mutex);
    auto lines = this->readAll();
    lines.push_back(line);

    size_t bytes = 0;
    for (const auto& l : lines) bytes += l.size() + 1;
    size_t first = 0;
    while (first + 1 < lines.size() && (lines.size() - first > this->maxLines || bytes > this->maxBytes)) {
        bytes -= lines[first].size() + 1;
        first++;
        this->overflow++;
    }
    lines.erase(lines.begin(), lines.begin() + first);
    this->writeAll(lines);
}

std::vector<std::string> HealthQueue::peek(size_t n) const {
    std::lock_guard<std::mutex> lock(this->mutex);
    auto lines = this->readAll();
    if (lines.size() > n) lines.resize(n);
    return lines;
}

void HealthQueue::drop(size_t n) {
    if (n == 0) return;
    std::lock_guard<std::mutex> lock(this->mutex);
    auto lines = this->readAll();
    lines.erase(lines.begin(), lines.begin() + std::min(n, lines.size()));
    this->writeAll(lines);
}

size_t HealthQueue::size() const {
    std::lock_guard<std::mutex> lock(this->mutex);
    return this->readAll().size();
}

size_t HealthQueue::overflowDropped() const {
    std::lock_guard<std::mutex> lock(this->mutex);
    return this->overflow;
}

}  // namespace telemetry
