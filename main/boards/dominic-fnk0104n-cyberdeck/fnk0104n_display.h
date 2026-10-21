#pragma once

#include "lvgl.h"

inline void Fnk0104nRoundArea(lv_event_t* event) {
    auto area = static_cast<lv_area_t*>(lv_event_get_param(event));
    // ST77922 needs four-column windows; software rotation 90 maps logical Y to panel X.
    area->y1 = (area->y1 >> 2) << 2;
    area->y2 = ((area->y2 >> 2) << 2) + 3;
}
