#include "telemetry/health_uploader.hpp"

#include <algorithm>

namespace telemetry {

UploadResult uploadPending(HealthQueue& queue, const Transport& send, size_t batchSize) {
    UploadResult res;
    size_t n = std::max<size_t>(1, batchSize);
    // Bounded loop: every iteration either removes events or stops.
    for (int guard = 0; guard < 1000; guard++) {
        auto lines = queue.peek(n);
        if (lines.empty()) return res;

        std::string body;
        for (const auto& l : lines) body += l + "\n";
        const int status = send(body);

        if (status >= 200 && status < 300) {
            queue.drop(lines.size());
            res.sent += lines.size();
        } else if (status == 400) {
            queue.drop(lines.size());
            res.discarded += lines.size();
        } else if (status == 413) {
            if (lines.size() == 1) {
                queue.drop(1);
                res.discarded++;
            } else {
                n = std::max<size_t>(1, lines.size() / 2);
            }
        } else {
            res.completed = false;
            return res;
        }
    }
    res.completed = false;
    return res;
}

}  // namespace telemetry
