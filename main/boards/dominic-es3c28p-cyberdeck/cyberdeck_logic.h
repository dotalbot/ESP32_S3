#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace cyberdeck {
inline std::optional<int> BatteryPercent(int millivolts) {
    if (millivolts < 2500 || millivolts > 4400)
        return std::nullopt;
    // shortcut: Li-ion open-circuit estimate, use a fuel gauge for accurate charge under load.
    constexpr int voltage[] = {2850, 3200, 3380, 3530, 3610, 3720, 3820, 3900, 3990, 4070, 4160};
    if (millivolts <= voltage[0])
        return 0;
    for (int i = 1; i < 11; ++i)
        if (millivolts < voltage[i])
            return (i - 1) * 10 +
                   (millivolts - voltage[i - 1]) * 10 / (voltage[i] - voltage[i - 1]);
    return 100;
}
inline std::optional<std::pair<int, int>> DecodeLargeTouch(const uint8_t* point,
                                                           bool mirror_x = false,
                                                           bool mirror_y = false) {
    if (!(point[0] & 0x80))
        return std::nullopt;
    const int x = ((point[0] & 0x3f) << 8) | point[1];
    const int y = ((point[2] & 0x3f) << 8) | point[3];
    if (x >= 320 || y >= 480)
        return std::nullopt;
    return std::pair<int, int>{mirror_x ? 319 - x : x, mirror_y ? 479 - y : y};
}
inline std::optional<time_t> ManualTime(int year, int month, int day, int hour, int minute) {
    if (year < 2024 || year > 2040 || month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 ||
        hour > 23 || minute < 0 || minute > 59)
        return std::nullopt;
    std::tm chosen{};
    chosen.tm_year = year - 1900;
    chosen.tm_mon = month - 1;
    chosen.tm_mday = day;
    chosen.tm_hour = hour;
    chosen.tm_min = minute;
    chosen.tm_isdst = -1;
    const time_t timestamp = std::mktime(&chosen);
    std::tm actual{};
    if (!localtime_r(&timestamp, &actual) || actual.tm_year != year - 1900 ||
        actual.tm_mon != month - 1 || actual.tm_mday != day || actual.tm_hour != hour ||
        actual.tm_min != minute)
        return std::nullopt;
    return timestamp;
}
inline const char* WeatherName(int code) {
    switch (code) {
        case 0:
            return "Clear";
        case 1:
            return "Mainly clear";
        case 2:
            return "Partly cloudy";
        case 3:
            return "Overcast";
        case 45:
        case 48:
            return "Fog";
        case 51:
        case 53:
        case 55:
        case 56:
        case 57:
            return "Drizzle";
        case 61:
        case 63:
        case 65:
        case 66:
        case 67:
            return "Rain";
        case 71:
        case 73:
        case 75:
        case 77:
        case 85:
        case 86:
            return "Snow";
        case 80:
        case 81:
        case 82:
            return "Showers";
        case 95:
        case 96:
        case 97:
        case 99:
            return "Storm";
        default:
            return nullptr;
    }
}
inline bool WeatherFresh(bool connected, bool fetched, uint32_t elapsed, time_t reading,
                         time_t now) {
    return connected && fetched && elapsed < 1800000 && now >= reading && now - reading < 1800;
}
inline void UpdateBleName(std::string& name, bool& complete, std::string_view received,
                          bool received_complete) {
    // A later scan response may name an initially unnamed advertisement.
    if (!received.empty() && (received_complete || !complete)) {
        name = received;
        complete = received_complete;
    }
}
inline std::pair<int, int> MapTouch(int x, int y, bool swap, bool mirror_x, bool mirror_y,
                                    int width = 320, int height = 240,
                                    bool software_rotate = false) {
    if (x < 0 || x >= (swap ? height : width) || y < 0 || y >= (swap ? width : height))
        return {-1, -1};
    if (swap)
        std::swap(x, y);
    x = mirror_x ? width - 1 - x : x;
    y = mirror_y ? height - 1 - y : y;
    // LVGL rotates pointer input itself; return panel coordinates for its 90-degree display.
    return software_rotate ? std::pair<int, int>{y, width - 1 - x} : std::pair<int, int>{x, y};
}
using TouchPoint = std::pair<int, int>;
struct TouchOrientation {
    bool swap;
    bool mirror_x;
    bool mirror_y;
};
constexpr std::array<TouchPoint, 3> kCalibrationTargets{{{30, 30}, {290, 30}, {30, 210}}};
inline std::array<TouchPoint, 3> CalibrationTargets(int width, int height) {
    return {{{30, 30}, {width - 30, 30}, {30, height - 30}}};
}
inline std::optional<TouchOrientation> CalibrateTouch(const std::array<TouchPoint, 3>& points,
                                                      int width = 320, int height = 240) {
    const int dx = points[1].first - points[0].first;
    const int dy = points[1].second - points[0].second;
    const bool swap = std::abs(dy) > std::abs(dx);
    TouchOrientation result{
        swap, (swap ? dy : dx) < 0,
        (swap ? points[2].first - points[0].first : points[2].second - points[0].second) < 0};
    for (int i = 0; i < 3; ++i) {
        auto mapped = MapTouch(points[i].first, points[i].second, result.swap, result.mirror_x,
                               result.mirror_y, width, height);
        auto targets = CalibrationTargets(width, height);
        if (mapped.first < 0 || std::abs(mapped.first - targets[i].first) > 40 ||
            std::abs(mapped.second - targets[i].second) > 40)
            return std::nullopt;
    }
    return result;
}
inline bool ValidWifiPassword(std::string_view password, bool open) {
    if (open)
        return password.empty();
    if (password.size() >= 8 && password.size() <= 63)
        return true;
    if (password.size() != 64)
        return false;
    for (char c : password) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
            return false;
    }
    return true;
}
constexpr int kColumns = 32;
constexpr int kRows = 18;
using Grid = std::array<uint8_t, kColumns * kRows>;
inline Grid NextLife(const Grid& cells) {
    Grid next{};
    for (int y = 0; y < kRows; ++y) {
        for (int x = 0; x < kColumns; ++x) {
            int neighbours = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0)
                        continue;
                    neighbours += cells[((y + dy + kRows) % kRows) * kColumns +
                                        (x + dx + kColumns) % kColumns] != 0;
                }
            }
            next[y * kColumns + x] =
                neighbours == 3 || (cells[y * kColumns + x] && neighbours == 2);
        }
    }
    return next;
}
}  // namespace cyberdeck
