#include "tick_parser.hpp"

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <spdlog/spdlog.h>
#include <cstdlib>
#include <cerrno>

static double get_number_or(const rapidjson::Value& obj, const char* key, double fallback) {
    if (!obj.HasMember(key)) return fallback;
    const auto& v = obj[key];
    if (v.IsNumber()) return v.GetDouble();
    if (v.IsString()) {
        char* end = nullptr;
        errno = 0;
        const char* raw = v.GetString();
        double parsed = std::strtod(raw, &end);
        if (errno == 0 && end != raw && *end == '\0') return parsed;
    }
    return fallback;
}

static int64_t get_int64_or(const rapidjson::Value& obj, const char* key, int64_t fallback) {
    if (!obj.HasMember(key)) return fallback;
    const auto& v = obj[key];
    if (v.IsInt64()) return v.GetInt64();
    if (v.IsInt()) return static_cast<int64_t>(v.GetInt());
    if (v.IsUint64()) return static_cast<int64_t>(v.GetUint64());
    if (v.IsUint()) return static_cast<int64_t>(v.GetUint());
    if (v.IsString()) {
        char* end = nullptr;
        errno = 0;
        const char* raw = v.GetString();
        long long parsed = std::strtoll(raw, &end, 10);
        if (errno == 0 && end != raw && *end == '\0') return static_cast<int64_t>(parsed);
    }
    return fallback;
}

static std::string get_string_or(const rapidjson::Value& obj, const char* key, const std::string& fallback) {
    return (obj.HasMember(key) && obj[key].IsString()) ? std::string(obj[key].GetString()) : fallback;
}

std::string TickData::to_json() const {
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> w(sb);

    w.StartObject();
    w.Key("symbol");       w.String(symbol.c_str());
    w.Key("price");        w.Double(price);
    w.Key("volume");       w.Double(volume);
    w.Key("timestamp_ms"); w.Int64(timestamp_ms);
    w.Key("side");         w.String(side.c_str());
    w.EndObject();

    return sb.GetString();
}

static bool parse_single_tick(const rapidjson::Value& v, TickData& out) {
    if (!v.IsObject()) return false;

    out.symbol = get_string_or(v, "symbol", get_string_or(v, "s", "XAUUSD"));

    out.price = get_number_or(v, "p", get_number_or(v, "price", -1.0));
    if (out.price < 0.0) {
        return false;
    }

    out.volume = get_number_or(v, "v", get_number_or(v, "volume", get_number_or(v, "q", 0.0)));
    out.timestamp_ms = get_int64_or(v, "t", get_int64_or(v, "timestamp_ms", get_int64_or(v, "timestamp", 0)));

    if (v.HasMember("sd") && v["sd"].IsString()) {
        std::string sd = v["sd"].GetString();
        out.side = (sd == "B" || sd == "b") ? "buy" : "sell";
    } else if (v.HasMember("side") && v["side"].IsString()) {
        std::string side = v["side"].GetString();
        out.side = (side == "B" || side == "b" || side == "buy" || side == "BUY") ? "buy" : "sell";
    } else {
        int64_t td = get_int64_or(v, "td", 0);
        out.side = (td >= 0) ? "buy" : "sell";
    }

    return true;
}

std::vector<TickData> TickParser::parse(const std::string& raw_message) {
    std::vector<TickData> ticks;

    rapidjson::Document doc;
    doc.Parse(raw_message.c_str(), raw_message.size());

    if (doc.HasParseError()) return ticks;

    auto parse_container = [&](const rapidjson::Value& container) {
        if (container.IsArray()) {
            ticks.reserve(ticks.size() + container.Size());
            for (auto& item : container.GetArray()) {
                TickData t;
                if (parse_single_tick(item, t)) ticks.push_back(std::move(t));
            }
        } else if (container.IsObject()) {
            TickData t;
            if (parse_single_tick(container, t)) ticks.push_back(std::move(t));
        }
    };

    if (doc.IsArray()) {
        parse_container(doc);
        return ticks;
    }

    if (!doc.IsObject()) return ticks;

    if (doc.HasMember("data")) {
        parse_container(doc["data"]);
    }

    if (doc.HasMember("tick")) {
        parse_container(doc["tick"]);
    }

    if (ticks.empty()) {
        TickData t;
        if (parse_single_tick(doc, t)) ticks.push_back(std::move(t));
    }

    return ticks;
}
