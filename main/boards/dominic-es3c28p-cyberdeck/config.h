#pragma once

// Reuse the matching upstream electrical configuration, without changing that board.
#include "../freenove-esp32s3-display-2.8-lcd/config.h"

#define TOUCH_RESET_PIN GPIO_NUM_18
#define TOUCH_SWAP_XY true
#define TOUCH_MIRROR_X false
#define TOUCH_MIRROR_Y true
// Initial weather location; change these together for your location.
#define WEATHER_LATITUDE "51.5074"
#define WEATHER_LONGITUDE "-0.1278"
#define WEATHER_LOCATION "London"
#define CYBERDECK_TIMEZONE "GMT0BST,M3.5.0/1,M10.5.0"
