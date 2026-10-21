#pragma once

#include <cstdint>
#include <ctime>

namespace cyberdeck {
constexpr int kClockStyles = 5;
constexpr int kClockWidth = 320;
constexpr int kClockHeight = 150;
const char* ClockStyleName(int style);
struct HomePalette {
    uint32_t background, foreground, rim, off_button;
};
constexpr HomePalette HomeColors(bool dark) {
    return dark ? HomePalette{0x071510, 0xcaf7df, 0x34594d, 0x23433d}
                : HomePalette{0xf8f4e9, 0x23433d, 0xd9d7c8, 0xe1e6dc};
}
// RGB565 rendering; a null time shows an honest unsynchronised state.
void RenderClock(uint16_t* pixels, int style, const std::tm* time);
void RenderHomeClock(uint16_t* pixels, const std::tm* time, int weather_code,
                     int width = kClockWidth, int height = kClockHeight, bool dark = false);
// Fixed 96 x 96 RGB565 icon; only Weather animates, Home remains calm.
void RenderWeatherIcon(uint16_t* pixels, int code, bool day, unsigned frame);
}  // namespace cyberdeck
