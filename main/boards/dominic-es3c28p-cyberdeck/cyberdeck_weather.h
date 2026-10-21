#pragma once

#include <cJSON.h>
#include <cmath>
#include <optional>
#include "cyberdeck_logic.h"

namespace cyberdeck {
inline bool WeatherNumber(const cJSON* value, double low, double high) {
    return cJSON_IsNumber(value) && std::isfinite(value->valuedouble) &&
           value->valuedouble >= low && value->valuedouble <= high;
}
struct WeatherRange {
    double high, low;
    time_t day;
};
inline std::optional<WeatherRange> ReadWeatherRange(const cJSON* high, const cJSON* low,
                                                    const cJSON* day) {
    if (!WeatherNumber(high, -100, 100) || !WeatherNumber(low, -100, 100) ||
        high->valuedouble < low->valuedouble || !WeatherNumber(day, 1700000000, 2147483647) ||
        day->valuedouble != std::floor(day->valuedouble))
        return std::nullopt;
    return WeatherRange{high->valuedouble, low->valuedouble, static_cast<time_t>(day->valuedouble)};
}
inline std::string WeatherDayLabel(time_t day, time_t now) {
    std::tm date{}, today{};
    if (!localtime_r(&day, &date))
        return "Forecast";
    if (now >= 1700000000 && localtime_r(&now, &today) && date.tm_year == today.tm_year &&
        date.tm_yday == today.tm_yday)
        return "Today";
    char label[24];
    strftime(label, sizeof(label), "%d %b", &date);
    return label;
}
}  // namespace cyberdeck
