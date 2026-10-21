#pragma once

#ifdef CONFIG_BOARD_TYPE_DOMINIC_FNK0104N_CYBERDECK
#include "../dominic-fnk0104n-cyberdeck/config.h"
#else
#include "config.h"
#define CYBERDECK_LARGE_SCREEN 0
#define BATTERY_ADC_CHANNEL ADC_CHANNEL_8
#define BATTERY_DIVIDER_RESISTOR 200000
#define BATTERY_FITTED_DEFAULT 0
#define SD_CLK GPIO_NUM_38
#define SD_CMD GPIO_NUM_40
#define SD_D0 GPIO_NUM_39
#define SD_D1 GPIO_NUM_41
#define SD_D2 GPIO_NUM_48
#define SD_D3 GPIO_NUM_47
#endif
