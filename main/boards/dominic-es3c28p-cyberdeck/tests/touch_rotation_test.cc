#include "../cyberdeck_logic.h"
#include "lvgl.h"

#include <cassert>
#include <cstdio>

int main() {
    lv_init();
    auto display = lv_display_create(320, 480);
    lv_display_set_rotation(display, LV_DISPLAY_ROTATION_90);
    // Upright with USB on the right: the opposite landscape from the first build.
    for (int x = 0; x < 480; ++x) {
        for (int y = 0; y < 320; ++y) {
            // Pixel position from esp_lvgl_port_disp_area_rotate, then panel mirroring.
            const int panel_x = DISPLAY_MIRROR_X ? 319 - y : y;
            const int panel_y = DISPLAY_MIRROR_Y ? x : 479 - x;
            if (panel_x != 319 - y || panel_y != x) {
                std::fprintf(stderr, "FAIL: USB-right display pixel (%d,%d) lands at (%d,%d)\n", x,
                             y, panel_x, panel_y);
                return 1;
            }
            const uint8_t payload[] = {static_cast<uint8_t>(0x80 | (panel_x >> 8)),
                                       static_cast<uint8_t>(panel_x),
                                       static_cast<uint8_t>(panel_y >> 8),
                                       static_cast<uint8_t>(panel_y),
                                       0,
                                       0,
                                       0};
            auto decoded = cyberdeck::DecodeLargeTouch(payload, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
            assert(decoded);
            auto mapped = cyberdeck::MapTouch(decoded->first, decoded->second, true, true, false,
                                              480, 320, true);
            lv_point_t point{mapped.first, mapped.second};
            lv_display_rotate_point(display, &point);  // Real LVGL pointer input processing.
            if (point.x != x || point.y != y) {
                std::fprintf(stderr, "FAIL: tap at (%d,%d) reaches (%d,%d)\n", x, y,
                             static_cast<int>(point.x), static_cast<int>(point.y));
                return 1;
            }
        }
    }
    assert((cyberdeck::MapTouch(0, 0, true, false, true) ==
            std::pair<int, int>{0, 239}));  // Original board has no LVGL rotation.
    lv_display_delete(display);
    lv_deinit();
    std::puts("PASS: all 153600 displayed pixels match their touch coordinates");
}
