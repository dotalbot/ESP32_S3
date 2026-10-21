#include "../cyberdeck_clock.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    using namespace cyberdeck;
    constexpr int count = kClockWidth * kClockHeight;
    std::vector<uint16_t> guarded(count + 2, 0xdead);
    auto pixels = guarded.data() + 1;
    std::set<std::vector<uint16_t>> faces;
    std::tm time{};
    time.tm_hour = 10;
    time.tm_min = 25;
    time.tm_sec = 42;
    for (int style = 0; style < kClockStyles; ++style) {
        RenderClock(pixels, style, &time);
        assert(guarded.front() == 0xdead && guarded.back() == 0xdead);
        faces.emplace(pixels, pixels + count);
        if (argc == 2) {
            const std::string path =
                std::string(argv[1]) + "/clock-" + std::to_string(style) + ".ppm";
            FILE* file = std::fopen(path.c_str(), "wb");
            assert(file);
            std::fprintf(file, "P6\n%d %d\n255\n", kClockWidth, kClockHeight);
            for (int i = 0; i < count; ++i) {
                const unsigned char rgb[] = {
                    static_cast<unsigned char>(((pixels[i] >> 11) & 31) * 255 / 31),
                    static_cast<unsigned char>(((pixels[i] >> 5) & 63) * 255 / 63),
                    static_cast<unsigned char>((pixels[i] & 31) * 255 / 31)};
                std::fwrite(rgb, 1, 3, file);
            }
            std::fclose(file);
        }
        const std::vector<uint16_t> before(pixels, pixels + count);
        time.tm_sec = 43;
        RenderClock(pixels, style, &time);
        assert(!std::equal(before.begin(), before.end(), pixels));
        time.tm_sec = 42;
        for (int hour : {0, 12, 23}) {
            for (int minute = 0; minute < 60; ++minute) {
                std::tm edge{};
                edge.tm_hour = hour;
                edge.tm_min = minute;
                edge.tm_sec = 59;
                RenderClock(pixels, style, &edge);
                assert(guarded.front() == 0xdead && guarded.back() == 0xdead);
            }
        }
        RenderClock(pixels, style, nullptr);
        const std::vector<uint16_t> unsynced(pixels, pixels + count);
        std::tm invalid{};
        invalid.tm_hour = 24;
        RenderClock(pixels, style, &invalid);
        assert(std::equal(unsynced.begin(), unsynced.end(), pixels));
    }
    assert(faces.size() == kClockStyles);
    RenderHomeClock(pixels, &time, 0);
    const std::vector<uint16_t> home(pixels, pixels + count);
    ++time.tm_sec;
    RenderHomeClock(pixels, &time, 0);
    assert(!std::equal(home.begin(), home.end(), pixels));
    for (int code : {-1, 0, 1, 2, 3, 45, 55, 65, 75, 85, 99}) {
        RenderHomeClock(pixels, &time, code);
        assert(guarded.front() == 0xdead && guarded.back() == 0xdead);
    }
    RenderHomeClock(pixels, nullptr, -1);
    const std::vector<uint16_t> unset(pixels, pixels + count);
    assert(home != unset);
    std::tm invalid_home{};
    invalid_home.tm_hour = 24;
    RenderHomeClock(pixels, &invalid_home, -1);
    assert(std::equal(unset.begin(), unset.end(), pixels));
    std::vector<uint16_t> large(480 * 210 + 2, 0xdead);
    RenderHomeClock(large.data() + 1, &time, 0, 480, 210);
    assert(large.front() == 0xdead && large.back() == 0xdead);
    assert(std::count(large.begin(), large.end(), 0xdead) == 2);
    const auto before_large = large;
    ++time.tm_sec;
    RenderHomeClock(large.data() + 1, &time, 3, 480, 210);
    assert(large != before_large && large.front() == 0xdead && large.back() == 0xdead);
    for (auto size : {std::pair{320, 150}, std::pair{480, 210}}) {
        const int n = size.first * size.second;
        std::vector<uint16_t> themed(n + 2, 0xdead);
        auto image = themed.data() + 1;
        RenderHomeClock(image, &time, 0, size.first, size.second, false);
        const auto light = themed;
        RenderHomeClock(image, &time, 0, size.first, size.second, true);
        assert(themed != light && themed.front() == 0xdead && themed.back() == 0xdead);
        assert(std::count(themed.begin(), themed.end(), 0xdead) == 2);
        assert(std::count_if(image, image + n, [](uint16_t p) {
                   return ((p >> 11) & 31) < 5 && ((p >> 5) & 63) < 10 && (p & 31) < 5;
               }) > n / 2);
        assert(std::count_if(image, image + n, [](uint16_t p) {
                   return ((p >> 11) & 31) > 20 && ((p >> 5) & 63) > 40;
               }) > 100);
        RenderHomeClock(image, nullptr, -1, size.first, size.second, true);
        assert(themed.front() == 0xdead && themed.back() == 0xdead);
        RenderHomeClock(image, &time, 0, size.first, size.second, false);
        assert(themed == light);  // Switching back restores the original light image exactly.
    }
    std::puts(
        "PASS: five faces, live seconds, time boundaries, dark/light Home and guarded buffers");
}
