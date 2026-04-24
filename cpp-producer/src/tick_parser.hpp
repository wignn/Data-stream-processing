#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct TickData {
    std::string symbol;
    double      price;
    double      volume;
    int64_t     timestamp_ms;
    std::string side;

    std::string to_json() const;
};

class TickParser {
public:
    static std::vector<TickData> parse(const std::string& raw_message);
};
