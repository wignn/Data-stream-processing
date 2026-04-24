#pragma once

#include <string>
#include <cstdlib>
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace config {

inline std::string trim_copy(std::string s) {
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

inline std::string get_env(const std::string& key, const std::string& fallback = "") {
    const char* val = std::getenv(key.c_str());
    return val ? trim_copy(std::string(val)) : fallback;
}

inline std::string require_env(const std::string& key) {
    const char* val = std::getenv(key.c_str());
    std::string parsed = val ? trim_copy(std::string(val)) : "";
    if (parsed.empty()) {
        throw std::runtime_error("Required environment variable not set: " + key);
    }
    return parsed;
}

struct AppConfig {
    std::string infoway_api_key;
    std::string kafka_broker;
    std::string kafka_topic;
    int         heartbeat_interval_sec;
    int         reconnect_delay_ms;
    int         kafka_wait_timeout_sec;

    static AppConfig from_env() {
        AppConfig cfg;
        cfg.infoway_api_key        = require_env("INFOWAY_API_KEY");
        cfg.kafka_broker           = get_env("KAFKA_BROKER", "kafka:9092");
        cfg.kafka_topic            = get_env("KAFKA_TOPIC", "xauusd.ticks");
        cfg.heartbeat_interval_sec = 30;
        cfg.reconnect_delay_ms     = 5000;
        cfg.kafka_wait_timeout_sec = 60;
        return cfg;
    }
};

}
