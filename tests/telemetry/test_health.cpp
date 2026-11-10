// Host-side tests for the pure telemetry units (no borealis/mpv/libnx).
// Build and run: scripts/test_telemetry.sh
#include <cstdio>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

#include "telemetry/health_config.hpp"
#include "telemetry/health_queue.hpp"
#include "telemetry/health_uploader.hpp"

using namespace telemetry;

static int failures = 0;
#define CHECK(cond)                                                                \
    do {                                                                           \
        if (!(cond)) {                                                             \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " " #cond "\n"; \
            failures++;                                                            \
        }                                                                          \
    } while (0)

static std::string tmp(const std::string& name) { return "/tmp/swf_health_" + name; }
static void write(const std::string& p, const std::string& s) { std::ofstream(p, std::ios::trunc) << s; }

static void queueBasics() {
    std::remove(tmp("q1").c_str());
    HealthQueue q(tmp("q1"));
    q.append("{\"a\":1}");
    q.append("{\"a\":2}");
    q.append("{\"a\":3}");
    CHECK(q.size() == 3);
    CHECK(q.peek(2).size() == 2 && q.peek(2)[0] == "{\"a\":1}");
    q.drop(2);
    CHECK(q.size() == 1 && q.peek(5)[0] == "{\"a\":3}");
    HealthQueue again(tmp("q1"));  // survives a "restart"
    CHECK(again.size() == 1);
    q.append("has\nnewline");  // would corrupt the NDJSON framing: ignored
    q.append("");
    CHECK(q.size() == 1);
}

static void queueCaps() {
    std::remove(tmp("q2").c_str());
    HealthQueue q(tmp("q2"), 1 << 20, 3);
    for (int i = 1; i <= 5; i++) q.append("{\"n\":" + std::to_string(i) + "}");
    CHECK(q.size() == 3);
    CHECK(q.peek(1)[0] == "{\"n\":3}");  // oldest dropped first
    CHECK(q.overflowDropped() == 2);

    std::remove(tmp("q3").c_str());
    HealthQueue b(tmp("q3"), 30, 100);  // byte cap
    for (int i = 0; i < 6; i++) b.append("{\"payload\":\"xxxxxx\"}");
    CHECK(b.size() >= 1 && b.size() <= 1);  // 20 bytes each, cap 30 keeps the newest only
}

struct FakeServer {
    std::vector<int> script;  // statuses returned in order, last one repeats
    std::vector<size_t> batchSizes;
    int call = 0;
    int operator()(const std::string& body) {
        size_t n = 0;
        for (char c : body) n += (c == '\n');
        batchSizes.push_back(n);
        int s = script[std::min<size_t>(call, script.size() - 1)];
        call++;
        return s;
    }
};

static void fill(HealthQueue& q, const std::string& name, int n) {
    std::remove(tmp(name).c_str());
    for (int i = 0; i < n; i++) q.append("{\"i\":" + std::to_string(i) + "}");
}
#define QUEUE(var, name, n) \
    std::remove(tmp(name).c_str()); \
    HealthQueue var(tmp(name)); \
    fill(var, name, n)

static void uploaderContract() {
    {  // 2xx drains everything in batches
        QUEUE(q, "u1", 7);
        FakeServer s{{200}, {}, 0};
        auto r = uploadPending(q, std::ref(s), 3);
        CHECK(r.sent == 7 && r.completed && q.size() == 0);
        CHECK((s.batchSizes == std::vector<size_t>{3, 3, 1}));
    }
    {  // 401 keeps the queue
        QUEUE(q, "u2", 4);
        FakeServer s{{401}, {}, 0};
        auto r = uploadPending(q, std::ref(s));
        CHECK(!r.completed && r.sent == 0 && q.size() == 4);
    }
    {  // 503 and network failure (0) keep the queue
        for (int code : {503, 0, 500}) {
            QUEUE(q, "u3", 2);
            FakeServer s{{code}, {}, 0};
            uploadPending(q, std::ref(s));
            CHECK(q.size() == 2);
        }
    }
    {  // 400 discards (malformed); processing continues
        QUEUE(q, "u4", 2);
        FakeServer s{{400, 200}, {}, 0};
        auto r = uploadPending(q, std::ref(s), 1);
        CHECK(r.discarded == 1 && r.sent == 1 && q.size() == 0);
    }
    {  // 413 halves the batch, then succeeds
        QUEUE(q, "u5", 8);
        FakeServer s{{413, 413, 200}, {}, 0};
        auto r = uploadPending(q, std::ref(s), 8);
        CHECK(r.sent == 8 && q.size() == 0);
        CHECK(s.batchSizes[0] == 8 && s.batchSizes[1] == 4 && s.batchSizes[2] == 2);
    }
    {  // a single oversized event is dropped on 413
        QUEUE(q, "u6", 1);
        FakeServer s{{413}, {}, 0};
        auto r = uploadPending(q, std::ref(s));
        CHECK(r.discarded == 1 && q.size() == 0);
    }
    {  // success then outage: sent events are gone, the rest stays
        QUEUE(q, "u7", 4);
        FakeServer s{{200, 503}, {}, 0};
        auto r = uploadPending(q, std::ref(s), 2);
        CHECK(r.sent == 2 && !r.completed && q.size() == 2);
    }
}

static void configLoading() {
    CHECK(!loadHealthConfig({"/nonexistent/health.json"}).enabled);  // no defaults in code

    write(tmp("c1"), R"({"endpoint":"http://nas:8089/v1/events","apiKey":"ah_switchfin_x","env":"dev"})");
    auto c = loadHealthConfig({"/nonexistent", tmp("c1")});
    CHECK(c.enabled && c.endpoint == "http://nas:8089/v1/events" && c.apiKey == "ah_switchfin_x" && c.env == "dev");

    write(tmp("c2"), R"({"endpoint":"http://nas:8089/v1/events"})");  // no key -> disabled
    CHECK(!loadHealthConfig({tmp("c2")}).enabled);

    write(tmp("c3"), R"({"endpoint":"http://x","apiKey":"k","enabled":false})");
    CHECK(!loadHealthConfig({tmp("c3")}).enabled);

    write(tmp("c4"), "{not json");  // invalid JSON never throws; next candidate is used
    write(tmp("c5"), R"({"endpoint":"http://x","api_key":"k"})");
    CHECK(loadHealthConfig({tmp("c4"), tmp("c5")}).enabled);

    write(tmp("c6"), R"({"endpoint":"http://x","apiKey":"k","env":"staging"})");  // unknown env ignored
    CHECK(loadHealthConfig({tmp("c6")}).env == "prod");
}

static void deviceId() {
    std::remove(tmp("dev").c_str());
    std::string a = loadOrCreateDeviceId(tmp("dev"));
    std::regex re("^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$");
    CHECK(std::regex_match(a, re));
    CHECK(loadOrCreateDeviceId(tmp("dev")) == a);  // stable across launches
    write(tmp("dev"), "garbage\n");  // corrupt file is replaced with a valid UUID
    CHECK(std::regex_match(loadOrCreateDeviceId(tmp("dev")), re));
}

int main() {
    queueBasics();
    queueCaps();
    uploaderContract();
    configLoading();
    deviceId();
    if (failures) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "telemetry host tests: all passed\n";
    return 0;
}
