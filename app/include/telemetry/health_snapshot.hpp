#pragma once

#include <string>

#include "telemetry/health_queue.hpp"

namespace telemetry {

/// Atomically replace the in-progress session snapshot (temp file + rename).
bool writeSnapshot(const std::string& path, const std::string& json);
/// Remove the snapshot, if any.
void clearSnapshot(const std::string& path);
/// A snapshot left behind means the app died mid-session: queue it with
/// end_reason "crash" (its ts_end is the last snapshot time) and remove it.
/// An unreadable snapshot is removed without queueing. Returns true if queued.
bool recoverSnapshot(const std::string& path, HealthQueue& queue);

}  // namespace telemetry
