#include "cyberdeck_clock.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

namespace cyberdeck {
namespace {
constexpr float kPi = 3.14159265f;
constexpr uint16_t Color(uint32_t rgb) {
    return ((rgb >> 8) & 0xf800) | ((rgb >> 5) & 0x07e0) | ((rgb >> 3) & 0x001f);
}
constexpr const char* kStyles[] = {"Neon", "Analogue", "Flip", "Orbit", "Words"};
// Compact 5x7 lettering for the retro faces; no extra font assets.
constexpr uint8_t kGlyphs[][5] = {
    {0x3e, 0x51, 0x49, 0x45, 0x3e}, {0x00, 0x42, 0x7f, 0x40, 0x00}, {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4b, 0x31}, {0x18, 0x14, 0x12, 0x7f, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3c, 0x4a, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03}, {0x36, 0x49, 0x49, 0x49, 0x36},
    {0x06, 0x49, 0x49, 0x29, 0x1e}, {0x7e, 0x11, 0x11, 0x11, 0x7e}, {0x7f, 0x49, 0x49, 0x49, 0x36},
    {0x3e, 0x41, 0x41, 0x41, 0x22}, {0x7f, 0x41, 0x41, 0x22, 0x1c}, {0x7f, 0x49, 0x49, 0x49, 0x41},
    {0x7f, 0x09, 0x09, 0x09, 0x01}, {0x3e, 0x41, 0x49, 0x49, 0x7a}, {0x7f, 0x08, 0x08, 0x08, 0x7f},
    {0x00, 0x41, 0x7f, 0x41, 0x00}, {0x20, 0x40, 0x41, 0x3f, 0x01}, {0x7f, 0x08, 0x14, 0x22, 0x41},
    {0x7f, 0x40, 0x40, 0x40, 0x40}, {0x7f, 0x02, 0x0c, 0x02, 0x7f}, {0x7f, 0x04, 0x08, 0x10, 0x7f},
    {0x3e, 0x41, 0x41, 0x41, 0x3e}, {0x7f, 0x09, 0x09, 0x09, 0x06}, {0x3e, 0x41, 0x51, 0x21, 0x5e},
    {0x7f, 0x09, 0x19, 0x29, 0x46}, {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7f, 0x01, 0x01},
    {0x3f, 0x40, 0x40, 0x40, 0x3f}, {0x1f, 0x20, 0x40, 0x20, 0x1f}, {0x3f, 0x40, 0x38, 0x40, 0x3f},
    {0x63, 0x14, 0x08, 0x14, 0x63}, {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43}};

struct Face {
    uint16_t* pixels;
    int width = kClockWidth, height = kClockHeight;
    void Pixel(int x, int y, uint16_t color) {
        if (x >= 0 && x < width && y >= 0 && y < height)
            pixels[y * width + x] = color;
    }
    void Rect(int x, int y, int w, int h, uint16_t color) {
        for (int row = std::max(0, y); row < std::min(height, y + h); ++row)
            for (int col = std::max(0, x); col < std::min(width, x + w); ++col)
                Pixel(col, row, color);
    }
    void Disc(int x, int y, int radius, uint16_t color) {
        for (int dy = -radius; dy <= radius; ++dy)
            for (int dx = -radius; dx <= radius; ++dx)
                if (dx * dx + dy * dy <= radius * radius)
                    Pixel(x + dx, y + dy, color);
    }
    void Line(int x, int y, int end_x, int end_y, int thickness, uint16_t color) {
        const int dx = std::abs(end_x - x), sx = x < end_x ? 1 : -1;
        const int dy = -std::abs(end_y - y), sy = y < end_y ? 1 : -1;
        int error = dx + dy;
        while (true) {
            Rect(x - thickness / 2, y - thickness / 2, thickness, thickness, color);
            if (x == end_x && y == end_y)
                break;
            const int twice = 2 * error;
            if (twice >= dy) {
                error += dy;
                x += sx;
            }
            if (twice <= dx) {
                error += dx;
                y += sy;
            }
        }
    }
    std::pair<int, int> Point(int x, int y, float angle, int radius) {
        return {x + static_cast<int>(std::lround(std::sin(angle) * radius)),
                y - static_cast<int>(std::lround(std::cos(angle) * radius))};
    }
    void Arc(int x, int y, int radius, float progress, int thickness, uint16_t color) {
        auto previous = Point(x, y, 0, radius);
        const int steps = std::clamp(static_cast<int>(progress * 180), 0, 180);
        for (int i = 1; i <= steps; ++i) {
            auto next = Point(x, y, i * kPi / 90, radius);
            Line(previous.first, previous.second, next.first, next.second, thickness, color);
            previous = next;
        }
    }
    void Text(std::string_view text, int x, int y, int scale, uint16_t color) {
        for (char ch : text) {
            const int index = ch >= '0' && ch <= '9'   ? ch - '0'
                              : ch >= 'A' && ch <= 'Z' ? ch - 'A' + 10
                                                       : -1;
            if (index >= 0) {
                for (int col = 0; col < 5; ++col)
                    for (int row = 0; row < 7; ++row)
                        if (kGlyphs[index][col] & (1 << row))
                            Rect(x + col * scale, y + row * scale, scale, scale, color);
            } else if (ch == ':') {
                Rect(x + 2 * scale, y + scale, scale, scale, color);
                Rect(x + 2 * scale, y + 5 * scale, scale, scale, color);
            }
            x += 6 * scale;
        }
    }
    void Center(std::string_view text, int y, int scale, uint16_t color) {
        Text(text, (width - static_cast<int>(text.size()) * 6 * scale + scale) / 2, y, scale,
             color);
    }
    void Digit(int digit, int x, int y, int width, int height, uint16_t lit, uint16_t unlit) {
        constexpr uint8_t masks[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};
        const int t = 5, half = height / 2;
        const int segments[][4] = {{x + t, y, width - 2 * t, t},
                                   {x + width - t, y + t, t, half - t},
                                   {x + width - t, y + half + t, t, half - 2 * t},
                                   {x + t, y + height - t, width - 2 * t, t},
                                   {x, y + half + t, t, half - 2 * t},
                                   {x, y + t, t, half - t},
                                   {x + t, y + half, width - 2 * t, t}};
        for (int i = 0; i < 7; ++i)
            Rect(segments[i][0], segments[i][1], segments[i][2], segments[i][3],
                 masks[digit] & (1 << i) ? lit : unlit);
    }
};
std::string NumberWord(int number) {
    constexpr const char* small[] = {"ZERO",    "ONE",     "TWO",       "THREE",    "FOUR",
                                     "FIVE",    "SIX",     "SEVEN",     "EIGHT",    "NINE",
                                     "TEN",     "ELEVEN",  "TWELVE",    "THIRTEEN", "FOURTEEN",
                                     "FIFTEEN", "SIXTEEN", "SEVENTEEN", "EIGHTEEN", "NINETEEN"};
    if (number < 20)
        return small[number];
    constexpr const char* tens[] = {"", "", "TWENTY", "THIRTY", "FORTY", "FIFTY"};
    return std::string(tens[number / 10]) +
           (number % 10 ? " " + std::string(small[number % 10]) : "");
}
}  // namespace

const char* ClockStyleName(int style) { return kStyles[std::clamp(style, 0, kClockStyles - 1)]; }

void RenderHomeClock(uint16_t* pixels, const std::tm* time, int weather_code, int width,
                     int height) {
    if (!pixels || width <= 0 || height <= 0)
        return;
    const auto paper = Color(0xf8f4e9), ink = Color(0x23433d), orange = Color(0xe99345);
    std::fill_n(pixels, width * height, paper);
    Face face{pixels, width, height};
    const float factor = height / 150.0f;
    auto scale = [factor](int n) { return static_cast<int>(std::lround(n * factor)); };
    const int cx = scale(85), cy = scale(75), type = width > 320 ? 3 : 2;
    face.Disc(cx, cy, scale(72), Color(0xd9d7c8));
    face.Disc(cx, cy, scale(70), paper);
    for (int i = 0; i < 60; ++i) {
        auto start = face.Point(cx, cy, i * kPi / 30, scale(i % 5 ? 65 : 61));
        auto end = face.Point(cx, cy, i * kPi / 30, scale(67));
        face.Line(start.first, start.second, end.first, end.second, i % 5 ? 1 : 2, ink);
    }
    face.Text("12", cx - 11 * type / 2, cy - scale(52), type, ink);
    face.Text("3", cx + scale(46), cy - 7 * type / 2, type, ink);
    face.Text("6", cx - 5 * type / 2, cy + scale(37), type, ink);
    face.Text("9", cx - scale(57), cy - 7 * type / 2, type, ink);
    if (time && time->tm_hour >= 0 && time->tm_hour < 24 && time->tm_min >= 0 &&
        time->tm_min < 60 && time->tm_sec >= 0 && time->tm_sec <= 60) {
        auto hand = [&](float progress, int length, int thickness, uint16_t color) {
            auto end = face.Point(cx, cy, progress * 2 * kPi, scale(length));
            face.Line(cx, cy, end.first, end.second, scale(thickness), color);
        };
        hand((time->tm_hour % 12 + time->tm_min / 60.0f) / 12, 34, 5, ink);
        hand((time->tm_min + time->tm_sec / 60.0f) / 60, 49, 3, ink);
        hand(time->tm_sec / 60.0f, 56, 1, orange);
        face.Disc(cx, cy, scale(4), orange);
    }
    const int wx = width > 320 ? 360 : 242, wy = scale(22);
    if (weather_code >= 0) {
        if (weather_code <= 2) {
            for (int i = 0; i < 8; ++i) {
                auto start = face.Point(wx, wy, i * kPi / 4, scale(16));
                auto end = face.Point(wx, wy, i * kPi / 4, scale(22));
                face.Line(start.first, start.second, end.first, end.second, 2, orange);
            }
            face.Disc(wx, wy, scale(11), orange);
        }
        if (weather_code >= 2) {
            const auto cloud = Color(0xa6b7ae);
            face.Disc(wx - scale(7), wy + scale(6), scale(11), cloud);
            face.Disc(wx + scale(7), wy + scale(2), scale(14), cloud);
            face.Disc(wx + scale(21), wy + scale(8), scale(9), cloud);
            face.Rect(wx - scale(7), wy + scale(7), scale(28), scale(10), cloud);
            if (weather_code >= 51) {
                for (int x = wx - scale(4); x < wx + scale(22); x += scale(10))
                    face.Line(x, wy + scale(21), x - 3, wy + scale(27), 2, ink);
            }
        }
    }
}

void RenderClock(uint16_t* pixels, int style, const std::tm* time) {
    if (!pixels)
        return;
    style = std::clamp(style, 0, kClockStyles - 1);
    constexpr uint32_t backgrounds[] = {0x080e20, 0xf3eddf, 0x0c1424, 0x080c20, 0x0e1820};
    Face face{pixels};
    std::fill_n(pixels, kClockWidth * kClockHeight, Color(backgrounds[style]));
    const uint16_t ink = Color(style == 1 ? 0x24313c : 0xe4f6ff);
    if (!time || time->tm_hour < 0 || time->tm_hour > 23 || time->tm_min < 0 || time->tm_min > 59 ||
        time->tm_sec < 0 || time->tm_sec > 60) {
        face.Center("TIME NOT SET", 48, 3, ink);
        return;
    }
    const int hour = time->tm_hour, minute = time->tm_min, second = time->tm_sec;
    char clock[12], seconds[12];
    std::snprintf(clock, sizeof(clock), "%02d:%02d", hour, minute);
    std::snprintf(seconds, sizeof(seconds), "%02d", second);
    const float seconds_progress = second / 60.0f;
    const float minutes_progress = (minute + seconds_progress) / 60;
    const float hours_progress = (hour % 12 + minutes_progress) / 12;
    if (style == 0) {
        const auto cyan = Color(0x40ecff), purple = Color(0xc279ff);
        for (int x = 0; x < 320; x += 20)
            face.Line(x, 0, x, 128, 1, Color(0x111b31));
        for (int y = 0; y < 128; y += 20)
            face.Line(0, y, 319, y, 1, Color(0x111b31));
        const int digits[] = {hour / 10, hour % 10, minute / 10, minute % 10};
        const int x[] = {30, 84, 166, 220};
        for (int i = 0; i < 4; ++i)
            face.Digit(digits[i], x[i], 22, 46, 76, cyan, Color(0x17313d));
        face.Disc(148, 46, 3, second % 2 ? purple : cyan);
        face.Disc(148, 76, 3, second % 2 ? purple : cyan);
        face.Center(std::string("SECONDS ") + seconds, 110, 2, purple);
        face.Rect(30, 130, static_cast<int>(260 * seconds_progress), 2, cyan);
    } else if (style == 1) {
        constexpr int cx = 101, cy = 67;
        face.Disc(cx, cy, 61, Color(0xd9d2c3));
        face.Disc(cx, cy, 58, Color(0xfffcf4));
        for (int i = 0; i < 60; ++i) {
            const float angle = i * kPi / 30;
            auto start = face.Point(cx, cy, angle, i % 5 ? 53 : 48);
            auto end = face.Point(cx, cy, angle, 55);
            face.Line(start.first, start.second, end.first, end.second, i % 5 ? 1 : 2, ink);
        }
        auto hand = [&](float progress, int length, int width, uint16_t color) {
            auto end = face.Point(cx, cy, progress * 2 * kPi, length);
            face.Line(cx, cy, end.first, end.second, width, color);
        };
        hand(hours_progress, 30, 5, ink);
        hand(minutes_progress, 43, 3, ink);
        hand(seconds_progress, 48, 1, Color(0xea6743));
        face.Disc(cx, cy, 4, Color(0xea6743));
        face.Text(clock, 182, 45, 3, ink);
        face.Text(std::string("SEC ") + seconds, 190, 81, 2, Color(0x9b6847));
        face.Text("LONDON", 190, 108, 1, ink);
    } else if (style == 2) {
        const int digits[] = {hour / 10, hour % 10, minute / 10, minute % 10};
        for (int i = 0; i < 4; ++i) {
            const int x = 24 + i * 68;
            face.Rect(x, 14, 62, 96, Color(0x233247));
            face.Rect(x + 2, 16, 58, 92, Color(0x192437));
            face.Digit(digits[i], x + 13, 25, 36, 74, ink, Color(0x1e2c40));
            face.Line(x, 61, x + 61, 61, 2, Color(0x080e18));
            face.Rect(x, 57, 3, 9, Color(0x69798b));
            face.Rect(x + 59, 57, 3, 9, Color(0x69798b));
        }
        face.Center(std::string("24 HOUR   SEC ") + seconds, 118, 1, Color(0xa5b8d0));
    } else if (style == 3) {
        const uint16_t accents[] = {Color(0xc184ff), Color(0x45e4e0), Color(0xffba66)};
        const float progress[] = {hours_progress, minutes_progress, seconds_progress};
        for (int i = 0; i < 3; ++i) {
            const int radius = 59 - i * 10;
            face.Arc(99, 67, radius, 1, 3, Color(0x1d2940));
            face.Arc(99, 67, radius, progress[i], 3, accents[i]);
            auto dot = face.Point(99, 67, progress[i] * 2 * kPi, radius);
            face.Disc(dot.first, dot.second, 3, accents[i]);
        }
        face.Text(seconds, 88, 61, 2, ink);
        face.Text(clock, 180, 33, 3, ink);
        face.Text("HOURS", 190, 68, 1, accents[0]);
        face.Text("MINUTES", 190, 87, 1, accents[1]);
        face.Text("SECONDS", 190, 106, 1, accents[2]);
    } else {
        face.Text("IT IS", 18, 12, 1, Color(0x6e919d));
        const auto hour_text = NumberWord(hour % 12 ? hour % 12 : 12);
        const auto minute_text = minute == 0   ? std::string("O CLOCK")
                                 : minute < 10 ? "OH " + NumberWord(minute)
                                               : NumberWord(minute);
        face.Center(hour_text, 32, 4, Color(0x7ee5cd));
        const int scale = minute_text.size() > 12 ? 2 : 3;
        face.Center(minute_text, 76, scale, ink);
        face.Center(hour < 6 || hour >= 21 ? "AT NIGHT"
                    : hour < 12            ? "IN THE MORNING"
                    : hour < 18            ? "IN THE AFTERNOON"
                                           : "IN THE EVENING",
                    112, 1, Color(0x6e919d));
        face.Rect(18, 130, static_cast<int>(284 * seconds_progress), 2, Color(0x7ee5cd));
    }
}
}  // namespace cyberdeck
