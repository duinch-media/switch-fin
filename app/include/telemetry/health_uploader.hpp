#pragma once

#include <functional>
#include <string>

#include "telemetry/health_queue.hpp"

namespace telemetry {

/// Sends one NDJSON body and returns the HTTP status, or 0 on a network failure.
using Transport = std::function<int(const std::string& ndjsonBody)>;

struct UploadResult {
    size_t sent = 0;     // events the server accepted (2xx)
    size_t discarded = 0;  // events removed because the server will never accept them
    bool completed = true;  // false when it stopped early and events remain for a later retry
};

/// Drain the queue in batches following the app-health client contract:
///   2xx -> delete batch          400 -> delete (malformed, would never pass)
///   413 -> halve the batch (delete a single oversized event)
///   401/403/5xx/network -> keep the queue and stop; the next trigger retries.
UploadResult uploadPending(HealthQueue& queue, const Transport& send, size_t batchSize = 50);

}  // namespace telemetry
