#include "telemetry/health_config.hpp"

#include <cstdio>
#include <fstream>
#include <random>
#include <regex>
#include <sstream>

#include <nlohmann/json.hpp>

namespace telemetry {

namespace {

const std::regex UUID_RE("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$");

std::string trim(std::string s) {
    const char* ws = " \t\r\n";
    s.erase(0, s.find_first_not_of(ws));
    s.erase(s.find_last_not_of(ws) + 1);
    return s;
}

std::string randomUuid() {
    std::random_device rd;
    std::mt19937_64 gen((static_cast<uint64_t>(rd()) << 32) ^ rd());
    std::uniform_int_distribution<int> nibble(0, 15);
    const char* hex = "0123456789abcdef";
    std::string out;
    for (int i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            out += '-';
        } else if (i == 14) {
            out += '4';
        } else if (i == 19) {
            out += hex[8 + nibble(gen) % 4];
        } else {
            out += hex[nibble(gen)];
        }
    }
    return out;
}

}  // namespace

std::vector<std::string> healthConfigPaths(const std::string& configDir) {
    std::vector<std::string> paths;
    paths.push_back(configDir + "/health.json");
#ifdef __SWITCH__
    paths.push_back("sdmc:/switch/Switchfin/health.json");
#endif
    paths.push_back("./health.json");
    return paths;
}

HealthConfig loadHealthConfig(const std::vector<std::string>& paths) {
    for (const auto& path : paths) {
        std::ifstream in(path);
        if (!in) continue;
        try {
            nlohmann::json j = nlohmann::json::parse(in);
            HealthConfig c;
            c.endpoint = trim(j.value("endpoint", std::string()));
            c.apiKey = trim(j.value("apiKey", j.value("api_key", std::string())));
            const std::string env = trim(j.value("env", std::string("prod")));
            if (env == "prod" || env == "beta" || env == "dev" || env == "test") c.env = env;
            c.enabled = j.value("enabled", true) && !c.endpoint.empty() && !c.apiKey.empty();
            return c;
        } catch (const std::exception&) {
            continue;  // unreadable JSON: try the next candidate, never crash the app
        }
    }
    return HealthConfig();
}

std::string loadOrCreateDeviceId(const std::string& path) {
    {
        std::ifstream in(path);
        std::string line;
        if (in && std::getline(in, line)) {
            line = trim(line);
            if (std::regex_match(line, UUID_RE)) return line;
        }
    }
    std::string id = randomUuid();
    std::ofstream out(path, std::ios::trunc);
    out << id << '\n';
    return id;
}

}  // namespace telemetry
