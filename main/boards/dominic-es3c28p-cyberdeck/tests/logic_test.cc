#include <cassert>
#include <string>
#include "../cyberdeck_logic.h"

int main() {
    using namespace cyberdeck;
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
