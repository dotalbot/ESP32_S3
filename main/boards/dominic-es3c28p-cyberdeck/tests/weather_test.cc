#include "../cyberdeck_clock.h"
#include "../cyberdeck_weather.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <memory>
#include <vector>

int main() {
    using namespace cyberdeck;
    auto parse = [](const char* input) {
        std::unique_ptr<cJSON, decltype(&cJSON_Delete)> json(cJSON_Parse(input), cJSON_Delete);
        return ReadWeatherRange(cJSON_GetObjectItemCaseSensitive(json.get(), "hi"),
                                cJSON_GetObjectItemCaseSensitive(json.get(), "lo"),
                                cJSON_GetObjectItemCaseSensitive(json.get(), "day"));
    };
    auto valid = parse("{\"hi\":13.0,\"lo\":7.7,\"day\":1792537200}");
    assert(valid && valid->high == 13 && valid->low == 7.7);
    // Old caches and partial/untrusted API responses keep current weather, without a false range.
    for (auto invalid :
         {"{}", "{\"hi\":null,\"lo\":7,\"day\":1792537200}",
          "{\"hi\":\"13\",\"lo\":7,\"day\":1792537200}", "{\"hi\":101,\"lo\":7,\"day\":1792537200}",
          "{\"hi\":13,\"lo\":-101,\"day\":1792537200}", "{\"hi\":7,\"lo\":13,\"day\":1792537200}",
          "{\"hi\":13,\"lo\":7,\"day\":0}", "{\"hi\":13,\"lo\":7,\"day\":1792537200.5}"})
        assert(!parse(invalid));
    cJSON bad{};
    bad.type = cJSON_Number;
    bad.valuedouble = INFINITY;
    assert(!WeatherNumber(&bad, -100, 100));
    setenv("TZ", "GMT0BST,M3.5.0/1,M10.5.0", 1);
    tzset();
    for (auto date : {std::pair{3, 29}, std::pair{7, 1}, std::pair{10, 25}, std::pair{12, 31}}) {
        const auto midnight = *ManualTime(2026, date.first, date.second, 0, 0);
        const auto evening = *ManualTime(2026, date.first, date.second, 23, 59);
        assert(WeatherDayLabel(midnight, evening) == "Today");
        assert(WeatherDayLabel(midnight, evening + 60) != "Today");
        assert(WeatherDayLabel(midnight, 0) != "Today");
    }
    std::vector<uint16_t> buffer(96 * 96 + 2, 0xdead);
    auto pixels = buffer.data() + 1;
    for (int code : {-1, 0, 1, 2, 3, 45, 48, 51, 61, 71, 77, 80, 85, 95, 99}) {
        for (bool day : {false, true}) {
            for (unsigned frame : {0u, 7u, 59u, 100000u}) {
                RenderWeatherIcon(pixels, code, day, frame);
                assert(buffer.front() == 0xdead && buffer.back() == 0xdead);
                assert(std::count(buffer.begin(), buffer.end(), 0xdead) == 2);
            }
        }
    }
    RenderWeatherIcon(pixels, 61, true, 0);
    const auto rain = buffer;
    RenderWeatherIcon(pixels, 61, true, 7);
    assert(buffer != rain);
    RenderWeatherIcon(pixels, 71, true, 0);
    assert(buffer != rain);
    RenderWeatherIcon(pixels, 0, true, 0);
    const auto sun = buffer;
    RenderWeatherIcon(pixels, 0, false, 0);
    assert(buffer != sun);
    std::puts(
        "PASS: forecast validation, offline dates across DST, weather icons and animation bounds");
}
