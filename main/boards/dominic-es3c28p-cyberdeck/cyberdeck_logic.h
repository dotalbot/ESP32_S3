#pragma once

#include <array>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace cyberdeck {
inline void UpdateBleName(std::string& name, bool& complete, std::string_view received,
                          bool received_complete) {
    // A later scan response may name an initially unnamed advertisement.
    if (!received.empty() && (received_complete || !complete)) {
        name = received;
        complete = received_complete;
    }
}
inline std::pair<int, int> MapTouch(int x, int y, bool swap, bool mirror_x, bool mirror_y) {
    if (x < 0 || x >= (swap ? 240 : 320) || y < 0 || y >= (swap ? 320 : 240))
        return {-1, -1};
    if (swap)
        std::swap(x, y);
    const int width = 320;
    const int height = 240;
    return {mirror_x ? width - 1 - x : x, mirror_y ? height - 1 - y : y};
}
using TouchPoint = std::pair<int, int>;
struct TouchOrientation {
    bool swap;
    bool mirror_x;
    bool mirror_y;
};
constexpr std::array<TouchPoint, 3> kCalibrationTargets{{{30, 30}, {290, 30}, {30, 210}}};
inline std::optional<TouchOrientation> CalibrateTouch(const std::array<TouchPoint, 3>& points) {
    const int dx = points[1].first - points[0].first;
    const int dy = points[1].second - points[0].second;
    const bool swap = std::abs(dy) > std::abs(dx);
    TouchOrientation result{
        swap, (swap ? dy : dx) < 0,
        (swap ? points[2].first - points[0].first : points[2].second - points[0].second) < 0};
    for (int i = 0; i < 3; ++i) {
        auto mapped = MapTouch(points[i].first, points[i].second, result.swap, result.mirror_x,
                               result.mirror_y);
        if (mapped.first < 0 || std::abs(mapped.first - kCalibrationTargets[i].first) > 40 ||
            std::abs(mapped.second - kCalibrationTargets[i].second) > 40)
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
