#include <cassert>
#include <string>
#include "../cyberdeck_logic.h"

int main() {
    using namespace cyberdeck;
    assert(!BatteryPercent(0) && !BatteryPercent(2499) && !BatteryPercent(4401));
    assert(BatteryPercent(2850) == 0 && BatteryPercent(3720) == 50 && BatteryPercent(4200) == 100);
    int previous = 0;
    for (int mv = 2500; mv <= 4400; ++mv) {
        auto level = BatteryPercent(mv);
        assert(level && *level >= previous && *level <= 100);
        previous = *level;
    }
    const uint8_t active[] = {0x81, 0x3f, 0x01, 0xdf, 0, 0, 0};
    assert((DecodeLargeTouch(active) == std::pair<int, int>{319, 479}));
    const uint8_t released[7]{};
    assert(!DecodeLargeTouch(released));
    const uint8_t malformed[] = {0xbf, 0xff, 0x3f, 0xff, 0, 0, 0};
    assert(!DecodeLargeTouch(malformed));
    assert((MapTouch(319, 479, true, false, true, 480, 320) == std::pair<int, int>{479, 0}));
    assert((MapTouch(320, 0, true, false, true, 480, 320) == std::pair<int, int>{-1, -1}));
    for (bool swap : {false, true}) {
        for (bool mx : {false, true}) {
            for (bool my : {false, true}) {
                auto points = CalibrationTargets(480, 320);
                for (auto& point : points) {
                    if (mx)
                        point.first = 479 - point.first;
                    if (my)
                        point.second = 319 - point.second;
                    if (swap)
                        std::swap(point.first, point.second);
                }
                auto orientation = CalibrateTouch(points, 480, 320);
                assert(orientation && orientation->swap == swap && orientation->mirror_x == mx &&
                       orientation->mirror_y == my);
            }
        }
    }
    setenv("TZ", "GMT0BST,M3.5.0/1,M10.5.0", 1);
    tzset();
    assert(ManualTime(2024, 2, 29, 12, 0));
    assert(!ManualTime(2026, 2, 29, 12, 0));
    assert(!ManualTime(2026, 4, 31, 12, 0));
    assert(!ManualTime(2026, 3, 29, 1, 30));  // Missing London hour at the spring change.
    assert(ManualTime(2026, 10, 25, 1, 30));
    assert(!ManualTime(2041, 1, 1, 0, 0));
    const auto summer = *ManualTime(2026, 7, 1, 12, 0);
    const auto winter = *ManualTime(2026, 1, 1, 12, 0);
    std::tm utc{};
    gmtime_r(&summer, &utc);
    assert(utc.tm_hour == 11);
    gmtime_r(&winter, &utc);
    assert(utc.tm_hour == 12);
    assert(WeatherName(0) == std::string("Clear"));
    assert(WeatherName(65) == std::string("Rain"));
    assert(WeatherName(99) == std::string("Storm"));
    assert(!WeatherName(4) && !WeatherName(-1));
    assert(WeatherFresh(true, true, 1000, summer, summer + 900));
    assert(!WeatherFresh(false, true, 1000, summer, summer + 900));
    assert(!WeatherFresh(true, false, 1000, summer, summer + 900));
    assert(!WeatherFresh(true, true, 1800000, summer, summer + 900));
    assert(!WeatherFresh(true, true, 1000, summer, summer + 1800));
    assert(!WeatherFresh(true, true, 1000, summer, summer - 1));
    std::string name;
    bool complete = false;
    UpdateBleName(name, complete, "", false);
    assert(name.empty());
    UpdateBleName(name, complete, "Sensor", false);
    assert(name == "Sensor" && !complete);
    UpdateBleName(name, complete, "Sensor full name", true);
    assert(name == "Sensor full name" && complete);
    UpdateBleName(name, complete, "", false);
    UpdateBleName(name, complete, "Sensor", false);
    assert(name == "Sensor full name" && complete);
    assert((MapTouch(0, 0, true, false, true) == std::pair<int, int>{0, 239}));
    assert((MapTouch(239, 319, true, false, true) == std::pair<int, int>{319, 0}));
    assert((MapTouch(240, 0, true, false, true) == std::pair<int, int>{-1, -1}));
    assert((MapTouch(0, 320, true, false, true) == std::pair<int, int>{-1, -1}));
    assert((MapTouch(-1, 0, true, false, true) == std::pair<int, int>{-1, -1}));
    assert((MapTouch(290, 30, false, false, false) == std::pair<int, int>{290, 30}));
    for (bool swap : {false, true}) {
        for (bool mx : {false, true}) {
            for (bool my : {false, true}) {
                std::array<TouchPoint, 3> raw{};
                for (int i = 0; i < 3; ++i) {
                    auto [x, y] = kCalibrationTargets[i];
                    if (mx)
                        x = 319 - x;
                    if (my)
                        y = 239 - y;
                    if (swap)
                        std::swap(x, y);
                    raw[i] = {x, y};
                }
                auto result = CalibrateTouch(raw);
                assert(result && result->swap == swap && result->mirror_x == mx &&
                       result->mirror_y == my);
            }
        }
    }
    assert(!CalibrateTouch({{{30, 30}, {30, 30}, {30, 30}}}));
    assert(!CalibrateTouch({{{500, 500}, {600, 500}, {500, 700}}}));
    assert(ValidWifiPassword("", true));
    assert(!ValidWifiPassword("password", true));
    assert(!ValidWifiPassword("1234567", false));
    assert(ValidWifiPassword("12345678", false));
    assert(ValidWifiPassword(std::string(63, 'x'), false));
    assert(ValidWifiPassword(std::string(64, 'A'), false));
    assert(!ValidWifiPassword(std::string(64, 'x'), false));
    assert(!ValidWifiPassword(std::string(65, 'A'), false));
    Grid cells{};
    // A blinker straddling the horizontal wrap boundary becomes vertical.
    cells[5 * kColumns + 31] = cells[5 * kColumns] = cells[5 * kColumns + 1] = 1;
    Grid expected{};
    expected[4 * kColumns] = expected[5 * kColumns] = expected[6 * kColumns] = 1;
    assert(NextLife(cells) == expected);
    assert(NextLife(expected) == cells);
    // A block crossing both boundaries is stable.
    cells.fill(0);
    cells[0] = cells[31] = cells[17 * kColumns] = cells[17 * kColumns + 31] = 1;
    assert(NextLife(cells) == cells);
    cells.fill(0);
    cells[0] = 1;
    assert(NextLife(cells) == Grid{});
}
