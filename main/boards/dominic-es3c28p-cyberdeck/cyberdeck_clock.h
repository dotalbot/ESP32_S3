#pragma once

#include <cstdint>
#include <ctime>

namespace cyberdeck {
constexpr int kClockStyles = 5;
constexpr int kClockWidth = 320;
constexpr int kClockHeight = 150;
const char* ClockStyleName(int style);
// RGB565 rendering; a null time shows an honest unsynchronised state.
void RenderClock(uint16_t* pixels, int style, const std::tm* time);
void RenderHomeClock(uint16_t* pixels, const std::tm* time, int weather_code,
                     int width = kClockWidth, int height = kClockHeight);
// Fixed 96 x 96 RGB565 icon; only Weather animates, Home remains calm.
void RenderWeatherIcon(uint16_t* pixels, int code, bool day, unsigned frame);
}  // namespace cyberdeck
