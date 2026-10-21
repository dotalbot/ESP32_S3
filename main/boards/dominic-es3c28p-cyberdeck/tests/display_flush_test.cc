#include "../../dominic-fnk0104n-cyberdeck/fnk0104n_display.h"

#include <array>
#include <cstdio>

LV_FONT_DECLARE(font_noto_sans_basic_16_4);

static bool invalid_window = false;
static int partial_windows = 0;

static void Flush(lv_display_t* display, const lv_area_t* area, uint8_t*) {
    // At rotation 90, logical Y becomes ST77922 physical X (including panel mirroring).
    const int x_start = 320 - area->y2 - 1, x_end = 320 - area->y1;
    if (x_start % 4 || x_end % 4 || x_start < 0 || x_end > 320) {
        std::fprintf(stderr, "FAIL: status update sends unaligned ST77922 columns %d..%d\n",
                     x_start, x_end);
        invalid_window = true;
    }
    if (lv_area_get_width(area) < 480)
        ++partial_windows;
    lv_display_flush_ready(display);
}

int main() {
    lv_init();
    auto display = lv_display_create(320, 480);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_rotation(display, LV_DISPLAY_ROTATION_90);
    std::array<uint16_t, 480 * 20> buffer{};
    lv_display_set_buffers(display, buffer.data(), nullptr, sizeof(buffer),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, Flush);
    lv_display_add_event_cb(display, Fnk0104nRoundArea, LV_EVENT_INVALIDATE_AREA, nullptr);
    auto wifi = lv_label_create(lv_screen_active());
    lv_obj_set_style_text_font(wifi, &font_noto_sans_basic_16_4, 0);
    lv_obj_set_pos(wifi, 10, 5);
    lv_label_set_text(wifi, LV_SYMBOL_WIFI " Offline");
    auto battery = lv_label_create(lv_screen_active());
    lv_obj_set_style_text_font(battery, &font_noto_sans_basic_16_4, 0);
    lv_obj_set_pos(battery, 360, 5);
    lv_label_set_text(battery, LV_SYMBOL_BATTERY_3 " ~78%");
    lv_refr_now(display);
    for (int y = 0; y < 320; ++y) {
        lv_obj_set_y(wifi, y);
        lv_obj_set_y(battery, y);
        lv_label_set_text(wifi, y % 2 ? LV_SYMBOL_WIFI " Connected" : LV_SYMBOL_WIFI " Offline");
        lv_label_set_text_fmt(battery, LV_SYMBOL_BATTERY_3 " ~%d%%", 60 + y % 40);
        lv_refr_now(display);
    }
    lv_display_delete(display);
    lv_deinit();
    if (invalid_window || !partial_windows)
        return 1;
    std::puts("PASS: real LVGL partial status updates obey ST77922 four-column alignment");
}
