#include "cyberdeck_display.h"
#include <material_symbols.h>
#include <wifi_manager.h>
#include "application.h"
#include "board.h"
#include "cyberdeck_clock.h"
#include "cyberdeck_config.h"
#include "cyberdeck_radio.h"
#include "cyberdeck_storage.h"
#include "settings.h"
#if CYBERDECK_LARGE_SCREEN
#include "../dominic-fnk0104n-cyberdeck/fnk0104n_display.h"
#endif

#include <esp_crt_bundle.h>
#include <esp_heap_caps.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_netif_sntp.h>
#include <esp_random.h>
#include <esp_sntp.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <cJSON.h>
#include <host/ble_hs.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>

#include <sys/time.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <type_traits>

LV_FONT_DECLARE(BUILTIN_TEXT_FONT);
LV_FONT_DECLARE(font_noto_sans_basic_30_4);
LV_FONT_DECLARE(font_noto_sans_basic_16_4);
LV_FONT_DECLARE(font_material_symbols_30_4);
static_assert(sizeof(WEATHER_CACHE_KEY) <= NVS_KEY_NAME_MAX_SIZE);
CyberdeckDisplay* CyberdeckDisplay::ble_owner_ = nullptr;

lv_obj_t* CyberdeckDisplay::Button(lv_obj_t* parent, const char* label, int x, int y, int w, int h,
                                   lv_event_cb_t callback, void* data, lv_event_code_t trigger) {
    auto button = lv_button_create(parent);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, w, h);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x18342d), 0);
    lv_obj_set_style_border_color(button, lv_color_hex(0x51e6a4), 0);
    lv_obj_set_style_border_width(button, 1, 0);
    auto text = lv_label_create(button);
    lv_label_set_text(text, label);
    lv_obj_center(text);
    lv_obj_add_event_cb(button, callback, trigger, data);
    return button;
}

void CyberdeckDisplay::SetupUI() {
    SpiLcdDisplay::SetupUI();
    DisplayLockGuard lock(this);
    if (!lock || root_)
        return;
#if CYBERDECK_LARGE_SCREEN
    lv_display_add_event_cb(display_, Fnk0104nRoundArea, LV_EVENT_INVALIDATE_AREA, nullptr);
#endif
    Settings settings("display");
    brightness_ = std::clamp(static_cast<int>(settings.GetInt("brightness", 60)), 10, 100);
    Board::GetInstance().GetBacklight()->SetBrightness(brightness_);
    setenv("TZ", CYBERDECK_TIMEZONE, 1);
    tzset();
    root_ = lv_obj_create(lv_layer_top());
    lv_obj_set_size(root_, width_, height_);
    lv_obj_set_pos(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_set_style_bg_color(root_, lv_color_hex(0x071510), 0);
    lv_obj_set_style_text_color(root_, lv_color_hex(0xcaf7df), 0);
    lv_obj_set_style_text_font(root_, &BUILTIN_TEXT_FONT, 0);
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    title_ = lv_label_create(root_);
    lv_obj_set_pos(title_, 10, 21);
    lv_obj_set_width(title_, 225);
    lv_label_set_long_mode(title_, LV_LABEL_LONG_DOT);
    header_home_ = Button(
        root_, "Home", 245, 8, 70, 34,
        [](lv_event_t* event) {
            static_cast<CyberdeckDisplay*>(lv_event_get_user_data(event))->Home();
        },
        this, LV_EVENT_PRESSED);
    wifi_status_ = lv_label_create(root_);
    lv_obj_set_pos(wifi_status_, 10, 2);
    lv_obj_set_style_text_font(wifi_status_, LV_FONT_DEFAULT, 0);
    battery_status_ = lv_label_create(root_);
    lv_obj_set_pos(battery_status_, 151, 2);
    lv_obj_set_style_text_font(battery_status_, LV_FONT_DEFAULT, 0);
    body_ = lv_obj_create(root_);
    lv_obj_set_pos(body_, 0, 45);
    lv_obj_set_size(body_, 320, 195);
    lv_obj_set_style_pad_all(body_, 0, 0);
    lv_obj_set_style_border_width(body_, 0, 0);
    lv_obj_set_style_bg_opa(body_, LV_OPA_TRANSP, 0);
    voice_home_ = Button(
        lv_layer_top(), "Home", 245, 4, 70, 34,
        [](lv_event_t* event) {
            static_cast<CyberdeckDisplay*>(lv_event_get_user_data(event))->Home();
        },
        this, LV_EVENT_PRESSED);
    lv_obj_set_style_text_font(voice_home_, &BUILTIN_TEXT_FONT, 0);
    voice_talk_ = Button(
        lv_layer_top(), "Talk / stop", 10, 4, 130, 34,
        [](lv_event_t*) { Application::GetInstance().ToggleChatState(); }, this);
    lv_obj_set_style_text_font(voice_talk_, &BUILTIN_TEXT_FONT, 0);
    if (width_ > 320) {
        lv_obj_set_pos(title_, 14, 28);
        lv_obj_set_width(title_, 335);
        lv_obj_set_pos(header_home_, width_ - 110, 17);
        lv_obj_set_size(header_home_, 100, 40);
        lv_obj_set_pos(voice_home_, width_ - 110, 10);
        lv_obj_set_size(voice_home_, 100, 40);
        lv_obj_set_size(voice_talk_, 160, 40);
        lv_obj_set_style_text_font(wifi_status_, &font_noto_sans_basic_16_4, 0);
        lv_obj_set_style_text_font(battery_status_, &font_noto_sans_basic_16_4, 0);
        lv_obj_set_y(wifi_status_, 5);
        lv_obj_set_y(battery_status_, 5);
    }
    last_touch_ = lv_tick_get();
    lv_timer_create(
        [](lv_timer_t* timer) {
            static_cast<CyberdeckDisplay*>(lv_timer_get_user_data(timer))->Tick();
        },
        100, this);
    Settings touch_settings("cyberdeck");
    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> weather(
        cJSON_Parse(touch_settings.GetString(WEATHER_CACHE_KEY).c_str()), cJSON_Delete);
    auto cached_time = cJSON_GetObjectItemCaseSensitive(weather.get(), "time");
    auto cached_temp = cJSON_GetObjectItemCaseSensitive(weather.get(), "temp");
    auto cached_code = cJSON_GetObjectItemCaseSensitive(weather.get(), "code");
    if (cJSON_IsNumber(cached_time) && std::isfinite(cached_time->valuedouble) &&
        cached_time->valuedouble >= 1700000000 && cached_time->valuedouble <= 2147483647 &&
        cJSON_IsNumber(cached_temp) && cJSON_IsNumber(cached_code)) {
        weather_time_ = static_cast<time_t>(cached_time->valuedouble);
        weather_temperature_ = cached_temp->valueint;
        weather_code_ = cached_code->valueint;
    }
    if (weather_time_ < 1700000000 || weather_temperature_ < -1000 || weather_temperature_ > 1000 ||
        !cyberdeck::WeatherName(weather_code_)) {
        weather_time_ = 0;
        weather_code_ = -1;
    }
    Open(Page::Home);
    touch_swap_ = touch_settings.GetInt("touch_swap", TOUCH_SWAP_XY);
    touch_mirror_x_ = touch_settings.GetInt("touch_mx", TOUCH_MIRROR_X);
    touch_mirror_y_ = touch_settings.GetInt("touch_my", TOUCH_MIRROR_Y);
    if (!touch_settings.GetInt("touch_ready", CYBERDECK_LARGE_SCREEN))
        StartCalibration();
}

void CyberdeckDisplay::AttachTouch(i2c_master_dev_handle_t touch, int points) {
    DisplayLockGuard lock(this);
    if (!lock)
        return;
    touch_ = touch;
    touch_points_ = std::clamp(points, 1, 10);
    auto input = lv_indev_create();
    lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(input, display_);
    lv_indev_set_user_data(input, this);
    lv_indev_set_read_cb(input, ReadTouch);
    lv_timer_set_period(lv_indev_get_read_timer(input), 10);
}

void CyberdeckDisplay::ShowStartupDuck() {
    DisplayLockGuard lock(this);
    if (!lock || !root_ || startup_panel_)
        return;
    // Native LVGL shapes keep the duck crisp without another image asset or framebuffer.
    auto shape = [](lv_obj_t* parent, int x, int y, int w, int h, uint32_t color,
                    int radius = LV_RADIUS_CIRCLE) {
        auto object = lv_obj_create(parent);
        lv_obj_remove_style_all(object);
        lv_obj_set_pos(object, x, y);
        lv_obj_set_size(object, w, h);
        lv_obj_set_style_bg_color(object, lv_color_hex(color), 0);
        lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(object, radius, 0);
        lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
        return object;
    };
    startup_panel_ = shape(lv_layer_top(), 0, 0, width_, height_, 0x071510, 0);
    auto label = lv_label_create(startup_panel_);
    lv_obj_set_style_text_font(label, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xffdc42), 0);
    lv_label_set_text(label, "Pocket Cyberdeck");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 35);
    startup_shadow_ = shape(startup_panel_, -65, 184, 50, 8, 0x254438);
    lv_obj_set_y(startup_shadow_, height_ - 56);
    startup_duck_ = shape(startup_panel_, -84, 112, 84, 70, 0x071510, 0);
    lv_obj_set_style_bg_opa(startup_duck_, LV_OPA_TRANSP, 0);
    shape(startup_duck_, 25, 63, 15, 6, 0xff9632, 3);  // Feet
    shape(startup_duck_, 49, 63, 15, 6, 0xff9632, 3);
    shape(startup_duck_, 3, 35, 22, 18, 0xffdc42);      // Tail
    shape(startup_duck_, 10, 30, 59, 35, 0xffdc42);     // Body
    shape(startup_duck_, 47, 6, 33, 35, 0xffe667);      // Head
    shape(startup_duck_, 71, 23, 13, 11, 0xff9632, 4);  // Bill
    shape(startup_duck_, 22, 37, 31, 19, 0xf4bc27);     // Wing
    shape(startup_duck_, 64, 17, 6, 7, 0x142820);       // Eye and glint
    shape(startup_duck_, 65, 18, 2, 2, 0xffffff);
    shape(startup_duck_, 62, 28, 8, 5, 0xffb45b);  // Cheek
    startup_started_ = lv_tick_get();
    ESP_LOGI("CyberdeckStartup", "Duck started: 2000 ms");
    lv_timer_create(
        [](lv_timer_t* timer) {
            auto self = static_cast<CyberdeckDisplay*>(lv_timer_get_user_data(timer));
            const uint32_t elapsed = lv_tick_get() - self->startup_started_;
            if (elapsed >= 2000) {
                lv_obj_delete(self->startup_panel_);
                self->startup_panel_ = nullptr;
                self->startup_duck_ = nullptr;
                self->startup_shadow_ = nullptr;
                self->ignore_touch_ = true;  // A held finger must release before using the menu.
                self->last_touch_ = lv_tick_get();
                lv_timer_delete(timer);
                self->Home();  // An existing calibration panel remains available on first boot.
                ESP_LOGI("CyberdeckStartup", "Duck complete: %lu ms; %s",
                         static_cast<unsigned long>(elapsed),
                         self->calibration_step_ >= 0 ? "calibration ready" : "home ready");
                return;
            }
            const float phase = (elapsed % 500) / 500.0f;
            const float hop = 4 * phase * (1 - phase);
            const int x = -84 + static_cast<int>((self->width_ + 84) * elapsed / 2000);
            lv_obj_set_pos(self->startup_duck_, x,
                           self->height_ - 128 - static_cast<int>(32 * hop));
            const int shadow_width = 50 - static_cast<int>(16 * hop);
            lv_obj_set_width(self->startup_shadow_, shadow_width);
            lv_obj_set_x(self->startup_shadow_, x + (84 - shadow_width) / 2);
        },
        33, this);
}

void CyberdeckDisplay::ReadTouch(lv_indev_t* input, lv_indev_data_t* data) {
    auto self = static_cast<CyberdeckDisplay*>(lv_indev_get_user_data(input));
    data->state = LV_INDEV_STATE_RELEASED;
    if (self->startup_panel_)
        return;
    int raw_x = -1, raw_y = -1;
#if CYBERDECK_LARGE_SCREEN
    const uint8_t info_reg[] = {0x00, 0x10};
    uint8_t info = 0;
    if (i2c_master_transmit_receive(self->touch_, info_reg, 2, &info, 1, 20) != ESP_OK)
        return;
    if (info & 0x08) {
        const uint8_t point_reg[] = {0x00, 0x14};
        uint8_t points[70]{};
        if (i2c_master_transmit_receive(self->touch_, point_reg, 2, points, self->touch_points_ * 7,
                                        20) != ESP_OK)
            return;
        for (int i = 0; i < self->touch_points_; ++i) {
            // Match the panel mounting before applying the saved touch calibration.
            auto point =
                cyberdeck::DecodeLargeTouch(points + i * 7, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
            if (point) {
                raw_x = point->first;
                raw_y = point->second;
                break;
            }
        }
    }
#else
    uint8_t reg = 0x02;
    uint8_t buf[5]{};
    if (i2c_master_transmit_receive(self->touch_, &reg, 1, buf, sizeof(buf), 50) != ESP_OK)
        return;
    if (buf[0] & 0x0f) {
        raw_x = ((buf[1] & 0x0f) << 8) | buf[2];
        raw_y = ((buf[3] & 0x0f) << 8) | buf[4];
    }
#endif
    if (raw_x < 0) {
        self->ignore_touch_ = false;
        return;
    }
    if (!self->screen_off_ && lv_tick_get() - self->last_touch_ >= 60000)
        Board::GetInstance().GetBacklight()->SetBrightness(self->brightness_);
    self->last_touch_ = lv_tick_get();
    if (self->screen_off_) {
        self->screen_off_ = false;
        self->ignore_touch_ = true;
        Board::GetInstance().GetBacklight()->SetBrightness(self->brightness_);
    }
    if (self->ignore_touch_)
        return;
    if (self->calibration_step_ >= 0) {
        self->ignore_touch_ = true;
        self->CaptureCalibration(raw_x, raw_y);
        return;
    }
    auto [x, y] = cyberdeck::MapTouch(raw_x, raw_y, self->touch_swap_, self->touch_mirror_x_,
                                      self->touch_mirror_y_, self->width_, self->height_,
                                      CYBERDECK_LARGE_SCREEN);
    if (x < 0)
        return;
    data->point.x = x;
    data->point.y = y;
    data->state = LV_INDEV_STATE_PRESSED;
}

void CyberdeckDisplay::Home() {
    DisplayLockGuard lock(this);
    if (!lock || !root_ || startup_panel_ || calibration_step_ >= 0)
        return;
    screen_off_ = false;
    last_touch_ = lv_tick_get();
    Board::GetInstance().GetBacklight()->SetBrightness(brightness_);
    Open(Page::Home);
}

void CyberdeckDisplay::ShowWifiSetup() {
    DisplayLockGuard lock(this);
    if (lock && root_ && !startup_panel_ && calibration_step_ < 0)
        Open(Page::WifiSetup);
}

bool CyberdeckDisplay::CheckRadioScreen(bool open) {
    DisplayLockGuard lock(this);
    if (!lock || !root_ || startup_panel_ || calibration_step_ >= 0)
        return false;
    if (open)
        Open(Page::Radio);
    lv_obj_update_layout(body_);
    return page_ == Page::Radio && !lv_obj_has_flag(root_, LV_OBJ_FLAG_HIDDEN) &&
           lv_obj_get_child_count(body_) == 8;
}

void CyberdeckDisplay::StartCalibration() {
    if (calibration_panel_)
        return;
    calibration_step_ = 0;
    ignore_touch_ = true;
    calibration_panel_ = lv_obj_create(lv_layer_top());
    lv_obj_set_size(calibration_panel_, width_, height_);
    lv_obj_set_pos(calibration_panel_, 0, 0);
    lv_obj_set_style_pad_all(calibration_panel_, 0, 0);
    lv_obj_set_style_border_width(calibration_panel_, 0, 0);
    lv_obj_set_style_radius(calibration_panel_, 0, 0);
    lv_obj_set_style_bg_color(calibration_panel_, lv_color_hex(0x071510), 0);
    lv_obj_set_style_text_color(calibration_panel_, lv_color_hex(0xcaf7df), 0);
    lv_obj_set_style_text_font(calibration_panel_, &BUILTIN_TEXT_FONT, 0);
    lv_obj_remove_flag(calibration_panel_, LV_OBJ_FLAG_SCROLLABLE);
    calibration_text_ = lv_label_create(calibration_panel_);
    lv_obj_set_width(calibration_text_, 220);
    lv_obj_set_style_text_align(calibration_text_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(calibration_text_, 50, 85);
    lv_label_set_text(calibration_text_,
                      "Touch calibration 1/3\nTap the red circle.\nLift your finger between taps.");
    calibration_target_ = lv_obj_create(calibration_panel_);
    lv_obj_set_size(calibration_target_, 24, 24);
    lv_obj_set_style_radius(calibration_target_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(calibration_target_, lv_color_hex(0xff5050), 0);
    lv_obj_set_style_border_width(calibration_target_, 0, 0);
    lv_obj_set_pos(calibration_target_, 18, 18);
}

void CyberdeckDisplay::CaptureCalibration(int x, int y) {
    ESP_LOGI("TouchCalibration", "target %d raw %d,%d", calibration_step_ + 1, x, y);
    calibration_points_[calibration_step_++] = {x, y};
    if (calibration_step_ == 3) {
        auto orientation = cyberdeck::CalibrateTouch(calibration_points_, width_, height_);
        if (!orientation) {
            calibration_step_ = 0;
            lv_label_set_text(
                calibration_text_,
                "Taps did not match.\nTap each red circle again.\nLift your finger between taps.");
        } else {
            touch_swap_ = orientation->swap;
            touch_mirror_x_ = orientation->mirror_x;
            touch_mirror_y_ = orientation->mirror_y;
            Settings settings("cyberdeck", true);
            settings.SetInt("touch_swap", touch_swap_);
            settings.SetInt("touch_mx", touch_mirror_x_);
            settings.SetInt("touch_my", touch_mirror_y_);
            settings.SetInt("touch_ready", 1);
            ESP_LOGI("TouchCalibration", "saved swap=%d mirror_x=%d mirror_y=%d", touch_swap_,
                     touch_mirror_x_, touch_mirror_y_);
            calibration_step_ = -1;
            lv_obj_delete(calibration_panel_);
            calibration_panel_ = nullptr;
            calibration_target_ = nullptr;
            calibration_text_ = nullptr;
            return;
        }
    } else {
        char prompt[100];
        snprintf(prompt, sizeof(prompt),
                 "Touch calibration %d/3\nTap the red circle.\nLift your finger between taps.",
                 calibration_step_ + 1);
        lv_label_set_text(calibration_text_, prompt);
    }
    auto [next_x, next_y] = cyberdeck::CalibrationTargets(width_, height_)[calibration_step_];
    lv_obj_set_pos(calibration_target_, next_x - 12, next_y - 12);
}

void CyberdeckDisplay::SetText(const std::string& text) {
    if (text_) {
        lv_obj_set_style_text_color(text_, lv_color_hex(0xcaf7df), LV_PART_MAIN);
        lv_label_set_text(text_, text.c_str());
    }
}

void CyberdeckDisplay::Open(Page page) {
    struct LayoutOnReturn {
        CyberdeckDisplay* display;
        ~LayoutOnReturn() { display->LayoutBody(); }
    } layout{this};
    body_scaled_ = false;
    if (page == Page::Voice || page == Page::WifiSetup || page == Page::Wifi) {
        Application::GetInstance().Schedule([]() { CyberdeckRadio::Instance().Stop(); });
    }
    ++request_;  // Invalidate results when the user leaves a screen.
    page_ = page;
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(voice_home_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(voice_talk_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clean(body_);
    lv_obj_add_flag(body_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_scroll_to_y(body_, 0, LV_ANIM_OFF);
    text_ = nullptr;
    canvas_ = nullptr;
    password_input_ = nullptr;
    radio_status_ = nullptr;
    radio_volume_ = nullptr;
    clock_default_label_ = nullptr;
    home_temperature_ = home_condition_ = home_weather_time_ = home_date_ = home_set_time_ =
        nullptr;
    home_buttons_.fill(nullptr);
    lv_obj_set_style_bg_color(root_, lv_color_hex(page == Page::Home ? 0xf8f4e9 : 0x071510), 0);
    lv_obj_set_style_text_color(root_, lv_color_hex(page == Page::Home ? 0x23433d : 0xcaf7df), 0);
    const int header = page == Page::Home ? (width_ > 320 ? 40 : 28) : (width_ > 320 ? 60 : 45);
    lv_obj_set_pos(body_, 0, header);
    lv_obj_set_size(body_, width_, height_ - header);
    if (page == Page::Home) {
        lv_obj_add_flag(header_home_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(title_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_x(battery_status_, width_ - (width_ > 320 ? 120 : 87));
    } else {
        lv_obj_remove_flag(header_home_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(title_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_x(battery_status_, width_ > 320 ? 250 : 151);
    }
    UpdateStatus();
    const char* names[] = {
        "Pocket Cyberdeck", "Wi-Fi survey",   "BLE discovery", "Clock",          "Weather",
        "Crypto prices",    "Matrix",         "Game of Life",  "Voice",          "Settings",
        "Wi-Fi setup",      "Wi-Fi password", "Joining Wi-Fi", "Internet Radio", "Apps",
        "Set time"};
    static_assert(std::size(names) == static_cast<size_t>(Page::SetTime) + 1);
    lv_label_set_text(title_, names[static_cast<int>(page)]);
    if (page == Page::Home) {
        ShowHome();
        return;
    }
    if (page == Page::SetTime) {
        ShowSetTime();
        return;
    }
    if (page == Page::Apps) {
        const char* apps[] = {"Wi-Fi",  "BLE",  "Clock", "Weather",  "Crypto",
                              "Matrix", "Life", "Voice", "Settings", "Radio"};
        for (int i = 0; i < 10; ++i) {
            auto button = Button(
                body_, apps[i], 7 + (i % 3) * 105, 3 + (i / 3) * 47, 96, 42,
                [](lv_event_t* event) {
                    auto self = static_cast<CyberdeckDisplay*>(lv_event_get_user_data(event));
                    auto page = static_cast<Page>(reinterpret_cast<intptr_t>(
                        lv_obj_get_user_data(lv_event_get_target_obj(event))));
                    self->Open(page);
                },
                this);
            lv_obj_set_user_data(button, reinterpret_cast<void*>(static_cast<intptr_t>(
                                             i == 9 ? Page::Radio : static_cast<Page>(i + 1))));
        }
        Button(
            body_, "Stop audio", 112, 144, 201, 42,
            [](lv_event_t*) {
                Application::GetInstance().Schedule([]() { CyberdeckRadio::Instance().Stop(); });
            },
            this);
        return;
    }
    if (page == Page::Radio) {
        ShowRadio();
        return;
    }
    if (page == Page::Clock) {
        Settings settings("cyberdeck");
        clock_style_ = std::clamp(static_cast<int>(settings.GetInt("clock_face", 0)), 0,
                                  cyberdeck::kClockStyles - 1);
        ShowClock();
        StartWork(Page::Clock);
        return;
    }
    if (page == Page::Voice) {
        lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(voice_home_, LV_OBJ_FLAG_HIDDEN);
        // The stock XiaoZhi UI and activation messages remain visible underneath.
        lv_obj_remove_flag(voice_talk_, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (page == Page::Matrix || page == Page::Life) {
        if (!pixels_)
            pixels_ = static_cast<uint16_t*>(heap_caps_calloc(width_ * 240, sizeof(uint16_t),
                                                              MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (!pixels_) {
            text_ = lv_label_create(body_);
            SetText("Animation needs PSRAM. Check board configuration.");
            return;
        }
        canvas_ = lv_canvas_create(body_);
        lv_canvas_set_buffer(canvas_, pixels_, 320, 180, LV_COLOR_FORMAT_RGB565);
        std::fill_n(pixels_, 320 * 180, 0);
        for (auto& y : rain_)
            y = -static_cast<int>(esp_random() % 180);
        for (auto& cell : life_)
            cell = esp_random() % 5 == 0;
        last_frame_ = 0;
        return;
    }
    text_ = lv_label_create(body_);
    lv_obj_set_pos(text_, 10, 5);
    lv_obj_set_width(text_, 298);
    lv_label_set_long_mode(text_, LV_LABEL_LONG_WRAP);
    if (page == Page::WifiPassword) {
        lv_obj_set_width(text_, 195);
        lv_label_set_long_mode(text_, LV_LABEL_LONG_DOT);
        SetText(selected_network_.ssid);
        Button(
            body_, "Connect", 205, 0, 105, 30,
            [](lv_event_t* event) {
                auto self = static_cast<CyberdeckDisplay*>(lv_event_get_user_data(event));
                self->ConnectSelected(lv_textarea_get_text(self->password_input_));
            },
            this);
        password_input_ = lv_textarea_create(body_);
        lv_obj_set_pos(password_input_, 8, 34);
        lv_obj_set_size(password_input_, 304, 34);
        lv_textarea_set_one_line(password_input_, true);
        lv_textarea_set_password_mode(password_input_, true);
        lv_textarea_set_max_length(password_input_, 64);
        lv_textarea_set_placeholder_text(password_input_, "Wi-Fi password");
        lv_obj_set_style_bg_color(password_input_, lv_color_hex(0x18342d), 0);
        lv_obj_set_style_text_color(lv_textarea_get_label(password_input_), lv_color_hex(0xffffff),
                                    LV_PART_MAIN);
        lv_obj_set_style_text_color(password_input_, lv_color_hex(0xa9c9bc),
                                    LV_PART_TEXTAREA_PLACEHOLDER);
        lv_obj_set_style_bg_color(password_input_, lv_color_hex(0xffffff), LV_PART_CURSOR);
        lv_obj_set_style_border_color(password_input_, lv_color_hex(0xffffff), LV_PART_CURSOR);
        lv_obj_set_style_text_color(password_input_, lv_color_hex(0x18342d), LV_PART_CURSOR);
        auto keyboard = lv_keyboard_create(body_);
        lv_obj_align(keyboard, LV_ALIGN_TOP_LEFT, 0, 88);
        lv_obj_set_size(keyboard, 320, 107);
        lv_obj_set_style_bg_color(keyboard, lv_color_hex(0x071510), LV_PART_MAIN);
        lv_obj_set_style_bg_color(keyboard, lv_color_hex(0x18342d), LV_PART_ITEMS);
        lv_obj_set_style_text_color(keyboard, lv_color_hex(0xcaf7df), LV_PART_ITEMS);
        lv_obj_set_style_pad_all(keyboard, 2, LV_PART_MAIN);
        lv_keyboard_set_textarea(keyboard, password_input_);
        lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
        lv_obj_add_event_cb(
            keyboard,
            [](lv_event_t* event) {
                auto self = static_cast<CyberdeckDisplay*>(lv_event_get_user_data(event));
                self->ConnectSelected(lv_textarea_get_text(self->password_input_));
            },
            LV_EVENT_READY, this);
        text_ = lv_label_create(body_);
        lv_obj_set_pos(text_, 8, 69);
        lv_obj_set_width(text_, 304);
        lv_label_set_long_mode(text_, LV_LABEL_LONG_DOT);
        SetText("Password: 8-63 characters.");
    } else if (page == Page::WifiJoinStatus) {
        SetText("Saving network and connecting...");
        last_clock_ = 0;
        Button(
            body_, "Try another network", 10, 145, 295, 40,
            [](lv_event_t* event) {
                static_cast<CyberdeckDisplay*>(lv_event_get_user_data(event))
                    ->Open(Page::WifiSetup);
            },
            this);
    } else if (page == Page::WifiSetup) {
        lv_obj_set_pos(text_, 10, 42);
        Button(
            body_, "Scan again", 195, 0, 115, 34,
            [](lv_event_t* event) {
                auto self = static_cast<CyberdeckDisplay*>(lv_event_get_user_data(event));
                self->StartWork(Page::WifiSetup);
            },
            this);
        StartWork(Page::WifiSetup);
    } else if (page == Page::Settings) {
        int level = 0;
        bool charging = false, discharging = false;
        const bool battery = Board::GetInstance().GetBatteryLevel(level, charging, discharging);
        char info[200];
        const std::string battery_text =
            battery ? "~" + std::to_string(level) + "%" : "not fitted / unavailable";
        snprintf(info, sizeof(info), "Battery: %s\nFree heap: %u KB\nSD: %s", battery_text.c_str(),
                 static_cast<unsigned>(esp_get_free_heap_size() / 1024), CyberdeckStorageStatus());
        SetText(info);
#if CYBERDECK_LARGE_SCREEN
        extern int CyberdeckBatteryMillivolts();
        const int mv = CyberdeckBatteryMillivolts();
        lv_label_set_text_fmt(text_, "Battery: %s  %.2f V\nFree heap: %u KB\nSD: %s",
                              battery_text.c_str(), mv / 1000.0,
                              static_cast<unsigned>(esp_get_free_heap_size() / 1024),
                              CyberdeckStorageStatus());
#endif
        auto slider = lv_slider_create(body_);
        lv_obj_set_pos(slider, 20, 93);
        lv_obj_set_size(slider, 270, 12);
        lv_slider_set_range(slider, 10, 100);
        lv_slider_set_value(slider, brightness_, LV_ANIM_OFF);
        lv_obj_add_event_cb(
            slider,
            [](lv_event_t* event) {
                auto self = static_cast<CyberdeckDisplay*>(lv_event_get_user_data(event));
                self->brightness_ = lv_slider_get_value(lv_event_get_target_obj(event));
                Board::GetInstance().GetBacklight()->SetBrightness(self->brightness_, true);
            },
            LV_EVENT_RELEASED, this);
        Button(
            body_, "Wi-Fi setup", 10, 125, 140, 45,
            [](lv_event_t* event) {
                auto self = static_cast<CyberdeckDisplay*>(lv_event_get_user_data(event));
                self->Open(Page::WifiSetup);
            },
            this);
        Button(
            body_, "Set date / time", 10, 232, 295, 40,
            [](lv_event_t* event) {
                static_cast<CyberdeckDisplay*>(lv_event_get_user_data(event))->Open(Page::SetTime);
            },
            this);
        Settings settings("cyberdeck");
        Button(
            body_,
            settings.GetInt("battery_ready", BATTERY_FITTED_DEFAULT) ? "Battery fitted: Yes"
                                                                     : "Battery fitted: No",
            10, 284, 295, 40,
            [](lv_event_t* event) {
                auto self = static_cast<CyberdeckDisplay*>(lv_event_get_user_data(event));
                {
                    Settings settings("cyberdeck", true);
                    settings.SetInt("battery_ready",
                                    !settings.GetInt("battery_ready", BATTERY_FITTED_DEFAULT));
                }
                self->Open(Page::Settings);
            },
            this);
        auto note = lv_label_create(body_);
        lv_obj_set_pos(note, 10, 334);
        lv_obj_set_width(note, 295);
        lv_obj_set_style_text_font(note, LV_FONT_DEFAULT, 0);
        lv_label_set_text(note,
                          "Enable only with a battery fitted. Percentage is an estimate; charging "
                          "status is unavailable.");
#if CYBERDECK_LARGE_SCREEN
        auto calibration_label = lv_label_create(body_);
        lv_obj_set_pos(calibration_label, 10, 395);
        lv_obj_set_style_text_font(calibration_label, LV_FONT_DEFAULT, 0);
        lv_label_set_text_fmt(calibration_label, "Battery calibration: %.1f%%",
                              settings.GetInt("batt_gain", 1000) / 10.0);
        auto calibration = lv_slider_create(body_);
        lv_obj_set_pos(calibration, 20, 433);
        lv_obj_set_size(calibration, 270, 12);
        lv_slider_set_range(calibration, 900, 1100);
        lv_slider_set_value(calibration, settings.GetInt("batt_gain", 1000), LV_ANIM_OFF);
        lv_obj_add_event_cb(
            calibration,
            [](lv_event_t* event) {
                {
                    Settings settings("cyberdeck", true);
                    settings.SetInt("batt_gain",
                                    lv_slider_get_value(lv_event_get_target_obj(event)));
                }
                static_cast<CyberdeckDisplay*>(lv_event_get_user_data(event))->Open(Page::Settings);
            },
            LV_EVENT_RELEASED, this);
#endif
        Button(
            body_, "Screen off", 165, 125, 140, 45,
            [](lv_event_t* event) {
                auto self = static_cast<CyberdeckDisplay*>(lv_event_get_user_data(event));
                self->screen_off_ = true;
                Board::GetInstance().GetBacklight()->SetBrightness(0);
            },
            this);
        Button(
            body_, "Calibrate touch", 10, 182, 295, 38,
            [](lv_event_t* event) {
                static_cast<CyberdeckDisplay*>(lv_event_get_user_data(event))->StartCalibration();
            },
            this);
    } else {
        SetText(page == Page::Wifi ? last_wifi_ : "Tap Refresh to load data.");
        auto refresh = Button(
            body_, "Refresh", 190, 0, 110, 38,
            [](lv_event_t* event) {
                auto self = static_cast<CyberdeckDisplay*>(lv_event_get_user_data(event));
                self->StartWork(self->page_);
            },
            this);
        (void)refresh;
        lv_obj_set_pos(text_, 10, 47);
        StartWork(page);
    }
}

void CyberdeckDisplay::LayoutBody() {
    if (body_scaled_ || width_ <= 320 || !body_ || page_ == Page::Home)
        return;
    body_scaled_ = true;
    lv_obj_update_layout(body_);
    for (uint32_t i = 0; i < lv_obj_get_child_count(body_); ++i) {
        auto child = lv_obj_get_child(body_, i);
        const int x = lv_obj_get_style_x(child, LV_PART_MAIN),
                  y = lv_obj_get_style_y(child, LV_PART_MAIN);
        if (child == canvas_) {
            lv_image_set_pivot(canvas_, 0, 0);
            lv_image_set_scale_x(canvas_, page_ == Page::Clock ? 341 : 384);
            lv_image_set_scale_y(canvas_, 341);
            lv_obj_set_pos(canvas_, page_ == Page::Clock ? 26 : 0, 0);
            continue;
        }
        const int w = lv_obj_get_style_width(child, LV_PART_MAIN),
                  h = lv_obj_get_style_height(child, LV_PART_MAIN);
        lv_obj_set_pos(child, x * width_ / 320, y * (height_ - 60) / 195);
        if (w > 0 && w <= 320)
            lv_obj_set_width(child, w * width_ / 320);
        if (h > 0 && h <= 400)
            lv_obj_set_height(child, h * (height_ - 60) / 195);
    }
}

void CyberdeckDisplay::Tick() {
    const uint32_t now = lv_tick_get();
    if (now - last_status_ >= 1000) {
        last_status_ = now;
        wifi_ap_record_t connected{};
        const bool online = esp_wifi_sta_get_ap_info(&connected) == ESP_OK;
        if (online && !was_connected_) {
            weather_attempt_ = 0;
            weather_fetched_ = false;
            time_sync_restart_ = true;
        }
        if (!online)
            weather_fetched_ = false;
        was_connected_ = online;
        UpdateStatus();
        if (online && !busy_ &&
            (!weather_attempt_ ||
             now - weather_attempt_ >= (weather_fetched_ ? 900000u : 60000u))) {
            weather_attempt_ = now;
            StartWork(Page::Home);
        }
        if (page_ == Page::Home)
            UpdateHome();
    }
    if (!screen_off_ && now - last_touch_ >= 60000) {
        if (page_ == Page::Home)
            Board::GetInstance().GetBacklight()->SetBrightness(std::min(brightness_, 15));
        else {
            screen_off_ = true;
            Board::GetInstance().GetBacklight()->SetBrightness(0);
        }
    }
    if (screen_off_)
        return;
    if (page_ == Page::Radio && radio_status_) {
        lv_label_set_text(radio_status_, CyberdeckRadio::Instance().Status().c_str());
    }
    if (page_ == Page::WifiJoinStatus && !join_pending_ && now - last_clock_ >= 1000) {
        last_clock_ = now;
        auto& wifi = WifiManager::GetInstance();
        if (wifi.IsConnected()) {
            SetText("Connected to:\n" + wifi.GetSsid() + "\nIP: " + wifi.GetIpAddress() +
                    "\n\nTime sync runs automatically.");
        } else if (now - join_started_ > 60000) {
            SetText(
                "Could not connect.\nCheck the password and signal.\nChoose the network again to "
                "retry.");
        } else {
            SetText("Connecting to:\n" + selected_network_.ssid + "\n\nPlease wait...");
        }
    }
    if (page_ == Page::Clock && now - last_clock_ >= 1000) {
        last_clock_ = now;
        UpdateClock();
    }
    if ((page_ != Page::Matrix && page_ != Page::Life) || !canvas_ || now - last_frame_ < 150)
        return;
    last_frame_ = now;
    if (page_ == Page::Life) {
        life_ = cyberdeck::NextLife(life_);
        for (int y = 0; y < 180; ++y) {
            for (int x = 0; x < 320; ++x) {
                pixels_[y * 320 + x] =
                    life_[(y / 10) * 32 + x / 10] && x % 10 < 9 && y % 10 < 9 ? 0x07e0 : 0;
            }
        }
    } else if (page_ == Page::Matrix) {
        for (int i = 0; i < 320 * 180; ++i)
            pixels_[i] = (pixels_[i] & 0x07e0) > 0x0040 ? pixels_[i] - 0x0040 : 0;
        // ponytail: pixel-glyph rain; use a font renderer if literal Matrix characters matter.
        for (int column = 0; column < 20; ++column) {
            rain_[column] += 6;
            if (rain_[column] >= 180)
                rain_[column] = -static_cast<int>(esp_random() % 90);
            for (int y = std::max(0, rain_[column] - 7); y < std::min(180, rain_[column]); ++y)
                for (int x = column * 16 + 4; x < column * 16 + 11; ++x)
                    if (esp_random() & 1)
                        pixels_[y * 320 + x] = 0x07e0;
        }
    }
    lv_obj_invalidate(canvas_);
}

void CyberdeckDisplay::UpdateStatus() {
    wifi_ap_record_t connection{};
    const bool connected = esp_wifi_sta_get_ap_info(&connection) == ESP_OK;
    lv_label_set_text(wifi_status_,
                      connected ? LV_SYMBOL_WIFI " Connected" : LV_SYMBOL_WIFI " Offline");
    int level = 0;
    bool charging = false, discharging = false;
    if (Board::GetInstance().GetBatteryLevel(level, charging, discharging)) {
        char battery[32];
        const char* icon = level > 85   ? LV_SYMBOL_BATTERY_FULL
                           : level > 60 ? LV_SYMBOL_BATTERY_3
                           : level > 35 ? LV_SYMBOL_BATTERY_2
                           : level > 10 ? LV_SYMBOL_BATTERY_1
                                        : LV_SYMBOL_BATTERY_EMPTY;
        snprintf(battery, sizeof(battery), "%s ~%d%%", icon, level);
        lv_label_set_text(battery_status_, battery);
    } else {
        lv_label_set_text(battery_status_, LV_SYMBOL_BATTERY_EMPTY " --");
    }
}

void CyberdeckDisplay::ShowHome() {
    lv_obj_remove_flag(body_, LV_OBJ_FLAG_SCROLLABLE);
    if (!pixels_)
        pixels_ = static_cast<uint16_t*>(
            heap_caps_calloc(width_ * 240, sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (pixels_) {
        canvas_ = lv_canvas_create(body_);
        lv_canvas_set_buffer(canvas_, pixels_, width_, width_ > 320 ? 210 : 150,
                             LV_COLOR_FORMAT_RGB565);
    }
    auto label = [&](int x, int y, int w, const lv_font_t* font) {
        auto object = lv_label_create(body_);
        lv_obj_set_pos(object, x, y);
        lv_obj_set_width(object, w);
        lv_obj_set_style_text_font(object, font, 0);
        lv_obj_set_style_text_align(object, LV_TEXT_ALIGN_CENTER, 0);
        return object;
    };
    const bool large = width_ > 320;
    const int wx = large ? 255 : 173, ww = large ? 210 : 140;
    const lv_font_t* small = large ? &font_noto_sans_basic_16_4 : LV_FONT_DEFAULT;
    home_temperature_ = label(wx, large ? 62 : 43, ww, &font_noto_sans_basic_30_4);
    home_condition_ = label(wx, large ? 110 : 87, ww, small);
    home_weather_time_ = label(wx, large ? 140 : 104, ww, small);
    home_date_ = label(wx, large ? 196 : 137, ww, small);
    home_set_time_ = Button(
        body_, "Set time", large ? 40 : 30, large ? 85 : 60, large ? 155 : 110, large ? 46 : 34,
        [](lv_event_t* event) {
            static_cast<CyberdeckDisplay*>(lv_event_get_user_data(event))->Open(Page::SetTime);
        },
        this);
    // Tapping a working dial opens the five clock styles; an unset dial offers manual time.
    if (canvas_) {
        lv_obj_add_flag(canvas_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(
            canvas_,
            [](lv_event_t* event) {
                lv_point_t point{};
                if (!lv_indev_active())
                    return;
                lv_indev_get_point(lv_indev_active(), &point);
                if (point.x < (CYBERDECK_LARGE_SCREEN ? 240 : 166))
                    static_cast<CyberdeckDisplay*>(lv_event_get_user_data(event))
                        ->Open(Page::Clock);
            },
            LV_EVENT_CLICKED, this);
    }
    const int button_width = large ? 64 : 52, gap = large ? 24 : 16;
    const int first_x = (width_ - 3 * button_width - 2 * gap) / 2;
    const int button_y = large ? 226 : 166, button_height = large ? 50 : 42;
    auto radio = home_buttons_[0] = Button(
        body_, MATERIAL_SYMBOLS_HEADPHONES, first_x, button_y, button_width, button_height,
        [](lv_event_t* event) {
            static_cast<CyberdeckDisplay*>(lv_event_get_user_data(event))->Open(Page::Radio);
        },
        this);
    lv_obj_set_style_bg_color(radio, lv_color_hex(0xe99345), 0);
    lv_obj_set_style_border_width(radio, 0, 0);
    lv_obj_set_style_text_color(radio, lv_color_hex(0x23433d), 0);
    lv_obj_set_style_text_font(radio, &font_material_symbols_30_4, 0);
    auto apps = home_buttons_[1] = Button(
        body_, "", first_x + button_width + gap, button_y, button_width, button_height,
        [](lv_event_t* event) {
            static_cast<CyberdeckDisplay*>(lv_event_get_user_data(event))->Open(Page::Apps);
        },
        this);
    lv_obj_set_style_bg_color(apps, lv_color_hex(0x23433d), 0);
    lv_obj_set_style_border_width(apps, 0, 0);
    lv_obj_set_style_text_color(apps, lv_color_hex(0xffffff), 0);
    // Native shapes give Apps a crisp grid without another icon font or bitmap.
    for (int i = 0; i < 9; ++i) {
        auto square = lv_obj_create(apps);
        lv_obj_remove_style_all(square);
        lv_obj_set_size(square, 6, 6);
        lv_obj_align(square, LV_ALIGN_CENTER, (i % 3 - 1) * 10, (i / 3 - 1) * 10);
        lv_obj_set_style_bg_color(square, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_bg_opa(square, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(square, 1, 0);
        lv_obj_remove_flag(square, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(square, LV_OBJ_FLAG_SCROLLABLE);
    }
    auto off = home_buttons_[2] = Button(
        body_, MATERIAL_SYMBOLS_POWER_SETTINGS_NEW, first_x + 2 * (button_width + gap), button_y,
        button_width, button_height,
        [](lv_event_t* event) {
            auto self = static_cast<CyberdeckDisplay*>(lv_event_get_user_data(event));
            self->screen_off_ = true;
            Board::GetInstance().GetBacklight()->SetBrightness(0);
        },
        this);
    lv_obj_set_style_bg_color(off, lv_color_hex(0xe1e6dc), 0);
    lv_obj_set_style_border_width(off, 0, 0);
    lv_obj_set_style_text_color(off, lv_color_hex(0x23433d), 0);
    lv_obj_set_style_text_font(off, &font_material_symbols_30_4, 0);
    UpdateHome();
    ESP_LOGI("CyberdeckHome",
             "Home ready: analogue, " WEATHER_LOCATION ", Radio / Apps / display-off icons");
}

void CyberdeckDisplay::UpdateHome() {
    if (page_ != Page::Home || !home_date_)
        return;
    const time_t timestamp = time(nullptr);
    struct tm local{};
    const bool valid = timestamp >= 1700000000 && localtime_r(&timestamp, &local);
    if (canvas_) {
        cyberdeck::RenderHomeClock(pixels_, valid ? &local : nullptr, weather_code_, width_,
                                   width_ > 320 ? 210 : 150);
        lv_obj_invalidate(canvas_);
    }
    if (valid)
        lv_obj_add_flag(home_set_time_, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_remove_flag(home_set_time_, LV_OBJ_FLAG_HIDDEN);
    char date[40];
    if (valid)
        strftime(date, sizeof(date), "%a %d %b", &local);
    else
        snprintf(date, sizeof(date), "Time not set");
    if (width_ > 320)
        lv_label_set_text_fmt(home_date_, WEATHER_LOCATION " · %s", date);
    else
        lv_label_set_text(home_date_, date);
    if (!weather_time_) {
        lv_label_set_text(home_temperature_, "--");
        lv_label_set_text(home_condition_, "Weather unavailable");
        lv_label_set_text(home_weather_time_, "Connect Wi-Fi");
        return;
    }
    char temperature[32];
    snprintf(temperature, sizeof(temperature), "%.0f C", weather_temperature_ / 10.0);
    lv_label_set_text(home_temperature_, temperature);
    lv_label_set_text(home_condition_, cyberdeck::WeatherName(weather_code_));
    struct tm weather_local{};
    char weather_stamp[32];
    localtime_r(&weather_time_, &weather_local);
    strftime(weather_stamp, sizeof(weather_stamp), "%d %b %H:%M", &weather_local);
    const bool fresh =
        cyberdeck::WeatherFresh(was_connected_, weather_fetched_, lv_tick_get() - weather_updated_,
                                weather_time_, timestamp);
    const std::string stamp = std::string(fresh ? "Updated\n" : "Cached\n") + weather_stamp;
    lv_label_set_text(home_weather_time_, stamp.c_str());
}

bool CyberdeckDisplay::CheckHomeScreen(bool open) {
    DisplayLockGuard lock(this);
    if (!lock || !root_ || startup_panel_)
        return false;
    if (open) {
        screen_off_ = false;
        last_touch_ = lv_tick_get();
        Board::GetInstance().GetBacklight()->SetBrightness(brightness_);
        Open(Page::Home);
    }
    const bool ready = page_ == Page::Home && canvas_ && home_temperature_ && home_date_ &&
                       home_buttons_[0] && home_buttons_[1] && home_buttons_[2] &&
                       lv_obj_has_flag(header_home_, LV_OBJ_FLAG_HIDDEN);
    if (ready) {
        UpdateStatus();
        UpdateHome();
        const bool fresh =
            cyberdeck::WeatherFresh(was_connected_, weather_fetched_,
                                    lv_tick_get() - weather_updated_, weather_time_, time(nullptr));
        lv_obj_update_layout(body_);
        lv_area_t radio_area{}, apps_area{}, off_area{}, condition_area{}, temperature_area{},
            date_area{};
        lv_obj_get_coords(home_buttons_[0], &radio_area);
        lv_obj_get_coords(home_buttons_[1], &apps_area);
        lv_obj_get_coords(home_buttons_[2], &off_area);
        lv_obj_get_coords(home_condition_, &condition_area);
        lv_obj_get_coords(home_temperature_, &temperature_area);
        lv_obj_get_coords(home_date_, &date_area);
        lv_font_glyph_dsc_t glyph{};
        const bool icons = lv_font_get_glyph_dsc(&font_material_symbols_30_4, &glyph, 0xf01f, 0) &&
                           lv_font_get_glyph_dsc(&font_material_symbols_30_4, &glyph, 0xf8c7, 0);
        const bool fit = icons && radio_area.x1 >= 0 && radio_area.y2 < height_ &&
                         apps_area.y2 < height_ && off_area.y2 < height_ &&
                         radio_area.x2 < apps_area.x1 && apps_area.x2 < off_area.x1 &&
                         off_area.x2 < width_ && temperature_area.y2 < condition_area.y1 &&
                         date_area.y2 < radio_area.y1;
        ESP_LOGI("CyberdeckHome", "Home screen ready: clock=%s; weather=%s; wifi=%s; layout=%s",
                 time(nullptr) >= 1700000000 ? "set" : "unset",
                 !weather_time_ ? "none"
                 : fresh        ? "fresh"
                                : "cached",
                 was_connected_ ? "connected" : "offline", fit ? "fit" : "overlap");
        ESP_LOGI("CyberdeckHome",
                 "Home controls: headphones, app grid, display off; weather=" WEATHER_LOCATION);
#if CYBERDECK_LARGE_SCREEN
        extern int CyberdeckBatteryMillivolts();
        ESP_LOGI("CyberdeckHome", "Battery reading: %d mV; %s; display=%dx%d",
                 CyberdeckBatteryMillivolts(), lv_label_get_text(battery_status_), width_, height_);
#endif
        return fit;
    }
    return ready;
}

void CyberdeckDisplay::ShowSetTime() {
    lv_obj_remove_flag(body_, LV_OBJ_FLAG_SCROLLABLE);
    const time_t timestamp = time(nullptr);
    struct tm local{};
    if (timestamp < 1700000000 || !localtime_r(&timestamp, &local)) {
        local.tm_year = 126;
        local.tm_mon = 0;
        local.tm_mday = 1;
    }
    const char* names[] = {"Year", "Month", "Day", "Hour", "Min"};
    const int first[] = {2024, 1, 1, 0, 0};
    const int last[] = {2040, 12, 31, 23, 59};
    const int selected[] = {local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, local.tm_hour,
                            local.tm_min};
    for (int i = 0; i < 5; ++i) {
        auto label = lv_label_create(body_);
        lv_obj_set_pos(label, 8 + i * 63, 1);
        lv_obj_set_style_text_font(label, LV_FONT_DEFAULT, 0);
        lv_label_set_text(label, names[i]);
        std::string options;
        for (int n = first[i]; n <= last[i]; ++n) {
            if (!options.empty())
                options += '\n';
            options += std::to_string(n);
        }
        auto roller = time_rollers_[i] = lv_roller_create(body_);
        lv_obj_set_pos(roller, 5 + i * 63, 23);
        lv_obj_set_width(roller, 59);
        lv_obj_set_style_text_font(roller, LV_FONT_DEFAULT, LV_PART_MAIN);
        lv_obj_set_style_text_font(roller, LV_FONT_DEFAULT, LV_PART_SELECTED);
        lv_obj_set_style_bg_color(roller, lv_color_hex(0x18342d), LV_PART_MAIN);
        lv_obj_set_style_text_color(roller, lv_color_hex(0xffffff), LV_PART_MAIN);
        lv_obj_set_style_bg_color(roller, lv_color_hex(0x2e6654), LV_PART_SELECTED);
        lv_obj_set_style_text_color(roller, lv_color_hex(0xffffff), LV_PART_SELECTED);
        lv_roller_set_options(roller, options.c_str(), LV_ROLLER_MODE_NORMAL);
        lv_roller_set_visible_row_count(roller, 3);
        lv_roller_set_selected(roller, std::clamp(selected[i], first[i], last[i]) - first[i],
                               LV_ANIM_OFF);
    }
    text_ = lv_label_create(body_);
    lv_obj_set_pos(text_, 10, 113);
    lv_obj_set_width(text_, 300);
    lv_obj_set_style_text_font(text_, LV_FONT_DEFAULT, 0);
    SetText("London time. Wi-Fi corrects it later.");
    Button(
        body_, "Save time", 10, 151, 300, 40,
        [](lv_event_t* event) {
            auto self = static_cast<CyberdeckDisplay*>(lv_event_get_user_data(event));
            auto chosen =
                cyberdeck::ManualTime(2024 + lv_roller_get_selected(self->time_rollers_[0]),
                                      1 + lv_roller_get_selected(self->time_rollers_[1]),
                                      1 + lv_roller_get_selected(self->time_rollers_[2]),
                                      lv_roller_get_selected(self->time_rollers_[3]),
                                      lv_roller_get_selected(self->time_rollers_[4]));
            if (!chosen) {
                self->SetText("Invalid date / daylight-saving hour.");
                return;
            }
            Application::GetInstance().Schedule([self, timestamp = *chosen]() {
                const timeval value{timestamp, 0};
                const bool saved = settimeofday(&value, nullptr) == 0;
                DisplayLockGuard lock(self);
                if (!lock)
                    return;
                if (saved)
                    self->Home();
                else if (self->page_ == Page::SetTime)
                    self->SetText("Could not set time. Try again.");
            });
        },
        this);
}

void CyberdeckDisplay::ShowClock() {
    lv_obj_clean(body_);
    body_scaled_ = false;
    lv_obj_remove_flag(body_, LV_OBJ_FLAG_SCROLLABLE);
    canvas_ = nullptr;
    text_ = nullptr;
    clock_default_label_ = nullptr;
    if (!pixels_)
        pixels_ = static_cast<uint16_t*>(
            heap_caps_calloc(width_ * 240, sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!pixels_) {
        text_ = lv_label_create(body_);
        lv_obj_set_width(text_, 298);
        SetText("Clock faces need PSRAM. Check board configuration.");
        return;
    }
    char title[60];
    snprintf(title, sizeof(title), "Clock %d/5: %s", clock_style_ + 1,
             cyberdeck::ClockStyleName(clock_style_));
    lv_label_set_text(title_, title);
    canvas_ = lv_canvas_create(body_);
    lv_canvas_set_buffer(canvas_, pixels_, cyberdeck::kClockWidth, cyberdeck::kClockHeight,
                         LV_COLOR_FORMAT_RGB565);
    text_ = lv_label_create(body_);
    lv_obj_set_pos(text_, 5, 132);
    lv_obj_set_width(text_, 310);
    lv_label_set_long_mode(text_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(text_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(text_, lv_color_hex(clock_style_ == 1 ? 0x24313c : 0xb5ccda), 0);
    for (int direction : {-1, 1}) {
        auto button = Button(
            body_, direction < 0 ? "<" : ">", direction < 0 ? 8 : 264, 157, 48, 34,
            [](lv_event_t* event) {
                auto self = static_cast<CyberdeckDisplay*>(lv_event_get_user_data(event));
                const int direction = static_cast<int>(reinterpret_cast<intptr_t>(
                    lv_obj_get_user_data(lv_event_get_target_obj(event))));
                self->clock_style_ = (self->clock_style_ + direction + cyberdeck::kClockStyles) %
                                     cyberdeck::kClockStyles;
                self->ShowClock();
            },
            this);
        lv_obj_set_user_data(button, reinterpret_cast<void*>(static_cast<intptr_t>(direction)));
    }
    auto save = Button(
        body_, "Set default", 66, 157, 188, 34,
        [](lv_event_t* event) {
            auto self = static_cast<CyberdeckDisplay*>(lv_event_get_user_data(event));
            Settings settings("cyberdeck", true);
            settings.SetInt("clock_face", self->clock_style_);
            lv_label_set_text(self->clock_default_label_, "Default saved");
            ESP_LOGI("CyberdeckClock", "Default saved: %d", self->clock_style_);
        },
        this);
    clock_default_label_ = lv_obj_get_child(save, 0);
    Settings settings("cyberdeck");
    if (settings.GetInt("clock_face", 0) == clock_style_)
        lv_label_set_text(clock_default_label_, "Default saved");
    last_clock_ = 0;
    UpdateClock();
    LayoutBody();
    ESP_LOGI("CyberdeckClock", "Face ready: %d (%s)", clock_style_,
             cyberdeck::ClockStyleName(clock_style_));
}

void CyberdeckDisplay::UpdateClock() {
    if (page_ != Page::Clock || !canvas_)
        return;
    const time_t timestamp = time(nullptr);
    struct tm local{};
    const bool valid = timestamp >= 1700000000 && localtime_r(&timestamp, &local);
    cyberdeck::RenderClock(pixels_, clock_style_, valid ? &local : nullptr);
    lv_obj_invalidate(canvas_);
    char date[60];
    if (valid) {
        strftime(date, sizeof(date), "%a %d %b  /  London", &local);
        lv_label_set_text(text_, date);
    } else {
        lv_label_set_text(text_, "Apps > Settings > Set date / time");
    }
    lv_obj_set_style_text_color(text_, lv_color_hex(clock_style_ == 1 ? 0x24313c : 0xb5ccda), 0);
}

void CyberdeckDisplay::ShowRadio() {
    lv_obj_remove_flag(body_, LV_OBJ_FLAG_SCROLLABLE);
    constexpr char kVolumeReadyKey[] = "radio_vol_init";
    static_assert(sizeof(kVolumeReadyKey) <= NVS_KEY_NAME_MAX_SIZE);
    Settings settings("cyberdeck", true);
    int volume = Board::GetInstance().GetAudioCodec()->output_volume();
    if (!settings.GetInt(kVolumeReadyKey, 0)) {
        volume = std::min(volume, 20);
        settings.SetInt(kVolumeReadyKey, 1);
        Application::GetInstance().Schedule(
            [volume]() { Board::GetInstance().GetAudioCodec()->SetOutputVolume(volume); });
    }
    radio_station_ = std::clamp(static_cast<int>(settings.GetInt("radio_station", 0)), 0,
                                CyberdeckRadio::kStationCount - 1);
    auto stations = lv_dropdown_create(body_);
    lv_obj_set_pos(stations, 8, 0);
    lv_obj_set_size(stations, 304, 35);
    lv_dropdown_clear_options(stations);
    for (int i = 0; i < CyberdeckRadio::kStationCount; ++i)
        lv_dropdown_add_option(stations, CyberdeckRadio::StationName(i), LV_DROPDOWN_POS_LAST);
    lv_dropdown_set_selected(stations, radio_station_);
    lv_obj_set_style_bg_color(stations, lv_color_hex(0x18342d), LV_PART_MAIN);
    lv_obj_set_style_text_color(stations, lv_color_hex(0xffffff), LV_PART_MAIN);
    auto list = lv_dropdown_get_list(stations);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x18342d), LV_PART_MAIN);
    lv_obj_set_style_text_color(list, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_text_color(list, lv_color_hex(0xffffff), LV_PART_SELECTED);
    lv_obj_set_style_text_font(list, &BUILTIN_TEXT_FONT, LV_PART_MAIN);
    lv_obj_add_event_cb(
        stations,
        [](lv_event_t* event) {
            auto self = static_cast<CyberdeckDisplay*>(lv_event_get_user_data(event));
            self->radio_station_ = lv_dropdown_get_selected(lv_event_get_target_obj(event));
        },
        LV_EVENT_VALUE_CHANGED, this);
    Button(
        body_, "Play", 8, 40, 96, 39,
        [](lv_event_t* event) {
            const auto station =
                static_cast<CyberdeckDisplay*>(lv_event_get_user_data(event))->radio_station_;
            Application::GetInstance().Schedule(
                [station]() { CyberdeckRadio::Instance().Start(station); });
        },
        this);
    Button(
        body_, "Stop", 112, 40, 96, 39,
        [](lv_event_t*) {
            Application::GetInstance().Schedule([]() { CyberdeckRadio::Instance().Stop(); });
        },
        this);
    Button(
        body_, "Test", 216, 40, 96, 39,
        [](lv_event_t*) {
            Application::GetInstance().Schedule([]() { CyberdeckRadio::Instance().Start(-1); });
        },
        this);
    radio_volume_ = lv_label_create(body_);
    lv_obj_set_pos(radio_volume_, 8, 86);
    lv_label_set_text_fmt(radio_volume_, "Vol %d", volume);
    auto slider = lv_slider_create(body_);
    lv_obj_set_pos(slider, 87, 94);
    lv_obj_set_size(slider, 214, 14);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, volume, LV_ANIM_OFF);
    lv_obj_add_event_cb(
        slider,
        [](lv_event_t* event) {
            const int volume = lv_slider_get_value(lv_event_get_target_obj(event));
            auto self = static_cast<CyberdeckDisplay*>(lv_event_get_user_data(event));
            lv_label_set_text_fmt(self->radio_volume_, "Vol %d", volume);
            Application::GetInstance().Schedule(
                [volume]() { Board::GetInstance().GetAudioCodec()->SetOutputVolume(volume); });
        },
        LV_EVENT_RELEASED, this);
    auto sleep = lv_dropdown_create(body_);
    lv_obj_set_pos(sleep, 8, 120);
    lv_obj_set_size(sleep, 304, 32);
    lv_dropdown_set_options(
        sleep, "Sleep timer: off\nSleep: 15 minutes\nSleep: 30 minutes\nSleep: 60 minutes");
    const auto minutes = CyberdeckRadio::Instance().SleepMinutes();
    lv_dropdown_set_selected(sleep, minutes == 60 ? 3 : minutes == 30 ? 2 : minutes == 15 ? 1 : 0);
    lv_obj_set_style_bg_color(sleep, lv_color_hex(0x18342d), LV_PART_MAIN);
    lv_obj_set_style_text_color(sleep, lv_color_hex(0xffffff), LV_PART_MAIN);
    auto sleep_list = lv_dropdown_get_list(sleep);
    lv_obj_set_style_bg_color(sleep_list, lv_color_hex(0x18342d), LV_PART_MAIN);
    lv_obj_set_style_text_color(sleep_list, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_text_color(sleep_list, lv_color_hex(0xffffff), LV_PART_SELECTED);
    lv_obj_set_style_text_font(sleep_list, &BUILTIN_TEXT_FONT, LV_PART_MAIN);
    lv_obj_add_event_cb(
        sleep,
        [](lv_event_t* event) {
            const int minutes[] = {0, 15, 30, 60};
            const int selected = lv_dropdown_get_selected(lv_event_get_target_obj(event));
            Application::GetInstance().Schedule([duration = minutes[selected]]() {
                CyberdeckRadio::Instance().SetSleepMinutes(duration);
            });
        },
        LV_EVENT_VALUE_CHANGED, this);
    radio_status_ = lv_label_create(body_);
    lv_obj_set_pos(radio_status_, 8, 159);
    lv_obj_set_width(radio_status_, 304);
    lv_label_set_long_mode(radio_status_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(radio_status_, CyberdeckRadio::Instance().Status().c_str());
}

void CyberdeckDisplay::StartWork(Page page) {
    if (busy_.exchange(true)) {
        if (page != Page::Home)
            SetText("Another request is finishing.\nTap Refresh shortly.");
        return;
    }
    if (page == Page::Home || page == Page::Weather)
        weather_fetched_ = false;
    if (page != Page::Home)
        SetText(page == Page::WifiSetup ? "Scanning..."
                : page == Page::Wifi
                    ? "Scanning...\nWi-Fi pauses briefly and reconnects automatically."
                    : "Working...\nYou can return Home.");
    struct Job {
        CyberdeckDisplay* display;
        Page page;
        uint64_t request;
    };
    auto job = new Job{this, page, request_};
    if (xTaskCreate(
            [](void* arg) {
                std::unique_ptr<Job> job(static_cast<Job*>(arg));
                job->display->RunWork(job->page, job->request);
                job.reset();
                vTaskDelete(nullptr);
            },
            "cyberdeck_worker", 8192, job, 3, nullptr) != pdPASS) {
        delete job;
        busy_ = false;
        if (page != Page::Home)
            SetText("Not enough memory. Try again.");
    }
}

void CyberdeckDisplay::RunWork(Page page, uint64_t request) {
    if (page == Page::WifiSetup) {
        ScanJoinNetworks(request);
        return;
    }
    wifi_ap_record_t connected = {};
    if (esp_wifi_sta_get_ap_info(&connected) == ESP_OK && !time_sync_started_) {
        esp_sntp_config_t time_config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
        time_sync_started_ = esp_netif_sntp_init(&time_config) == ESP_OK;
        if (time_sync_started_)
            time_sync_restart_ = false;
    } else if (time_sync_started_ && time_sync_restart_.exchange(false)) {
        esp_sntp_restart();
    }
    std::string result;
    if (page == Page::Clock)
        result =
            "Waiting for time sync...\nConnect Wi-Fi from Settings.\nReopen Clock after "
            "connecting.";
    else if (page == Page::Wifi)
        result = ScanWifi();
    else if (page == Page::Ble)
        result = ScanBle();
    else
        result = FetchDashboard(page);
    Application::GetInstance().Schedule(
        [this, page, request, result = std::move(result)]() { Finish(page, request, result); });
}

void CyberdeckDisplay::Finish(Page page, uint64_t request, std::string result) {
    DisplayLockGuard lock(this);
    busy_ = false;
    if (!lock)
        return;
    if (page == Page::Wifi)
        last_wifi_ = result;
    if (page_ == page && request_ == request) {
        if (page == Page::Home)
            UpdateHome();
        else if (page == Page::Clock)
            UpdateClock();
        else
            SetText(result);
    }
}

void CyberdeckDisplay::ScanJoinNetworks(uint64_t request) {
    extern bool CyberdeckBeginWifiJoin();
    std::string status;
    std::vector<JoinNetwork> networks;
    if (!CyberdeckBeginWifiJoin()) {
        status = "Radio not ready. Try Scan again.";
    } else {
        wifi_scan_config_t config = {};
        config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
        config.scan_time.active.min = 80;
        config.scan_time.active.max = 140;
        auto error = esp_wifi_scan_start(&config, true);
        std::array<wifi_ap_record_t, 24> records{};
        uint16_t count = records.size();
        if (error == ESP_OK)
            error = esp_wifi_scan_get_ap_records(&count, records.data());
        if (error != ESP_OK) {
            esp_wifi_clear_ap_list();
            status = std::string("Scan failed: ") + esp_err_to_name(error);
        } else {
            std::sort(records.begin(), records.begin() + count,
                      [](const auto& a, const auto& b) { return a.rssi > b.rssi; });
            for (int i = 0; i < count; ++i) {
                auto& record = records[i];
                if (!record.ssid[0])
                    continue;
                std::string ssid(reinterpret_cast<const char*>(record.ssid),
                                 strnlen(reinterpret_cast<const char*>(record.ssid), 32));
                if (std::any_of(networks.begin(), networks.end(),
                                [&](const auto& n) { return n.ssid == ssid; }))
                    continue;
                networks.push_back(
                    {ssid, record.primary, record.rssi,
                     record.authmode == WIFI_AUTH_OPEN || record.authmode == WIFI_AUTH_OWE,
                     record.authmode != WIFI_AUTH_WEP &&
                         record.authmode != WIFI_AUTH_WPA2_ENTERPRISE});
            }
            if (networks.empty())
                status = "No named 2.4GHz networks found.\nTry Scan again.";
        }
    }
    Application::GetInstance().Schedule([this, request, networks = std::move(networks), status]() {
        DisplayLockGuard lock(this);
        busy_ = false;
        if (!lock || page_ != Page::WifiSetup || request_ != request)
            return;
        join_networks_ = networks;
        if (!status.empty())
            SetText(status);
        else
            ShowJoinNetworks();
    });
}

void CyberdeckDisplay::ShowJoinNetworks() {
    lv_obj_clean(body_);
    body_scaled_ = false;
    password_input_ = nullptr;
    text_ = lv_label_create(body_);
    lv_obj_set_pos(text_, 8, 8);
    lv_obj_set_width(text_, 180);
    lv_label_set_text(text_, "Select a 2.4GHz network");
    Button(
        body_, "Scan again", 195, 0, 115, 34,
        [](lv_event_t* event) {
            auto self = static_cast<CyberdeckDisplay*>(lv_event_get_user_data(event));
            self->StartWork(Page::WifiSetup);
        },
        this);
    for (size_t i = 0; i < join_networks_.size(); ++i) {
        const auto& network = join_networks_[i];
        const std::string label = network.ssid + " | " + std::to_string(network.rssi) + " dBm";
        auto button = Button(
            body_, label.c_str(), 8, 40 + i * 47, 304, 42,
            [](lv_event_t* event) {
                auto self = static_cast<CyberdeckDisplay*>(lv_event_get_user_data(event));
                const size_t index = reinterpret_cast<uintptr_t>(
                                         lv_obj_get_user_data(lv_event_get_target_obj(event))) -
                                     1;
                self->ChooseNetwork(index);
            },
            this);
        lv_obj_set_user_data(button, reinterpret_cast<void*>(i + 1));
        auto label_obj = lv_obj_get_child(button, 0);
        lv_obj_set_width(label_obj, 275);
        lv_label_set_long_mode(label_obj, LV_LABEL_LONG_DOT);
        lv_obj_center(label_obj);
    }
    LayoutBody();
}

void CyberdeckDisplay::ChooseNetwork(size_t index) {
    if (busy_ || index >= join_networks_.size())
        return;
    selected_network_ = join_networks_[index];
    if (!selected_network_.supported) {
        SetText("Unsupported security");
        return;
    }
    if (selected_network_.open)
        ConnectSelected("");
    else
        Open(Page::WifiPassword);
}

void CyberdeckDisplay::ConnectSelected(std::string password) {
    if (!cyberdeck::ValidWifiPassword(password, selected_network_.open)) {
        SetText("Use 8-63 characters or 64 hex digits.");
        return;
    }
    if (busy_.exchange(true))
        return;
    struct JoinJob {
        CyberdeckDisplay* display;
        JoinNetwork network;
        std::string password;
        uint64_t request;
    };
    Open(Page::WifiJoinStatus);
    join_started_ = lv_tick_get();
    join_pending_ = true;
    auto job = new JoinJob{this, selected_network_, std::move(password), request_};
    if (xTaskCreate(
            [](void* arg) {
                std::unique_ptr<JoinJob> job(static_cast<JoinJob*>(arg));
                extern void CyberdeckConnectWifi(const std::string&, const std::string&, uint8_t);
                CyberdeckConnectWifi(job->network.ssid, job->password, job->network.channel);
                auto self = job->display;
                const auto request = job->request;
                Application::GetInstance().Schedule([self, request]() {
                    DisplayLockGuard lock(self);
                    self->busy_ = false;
                    self->join_pending_ = false;
                    if (lock && self->page_ == Page::WifiJoinStatus && self->request_ == request)
                        self->SetText("Connecting... Please wait.");
                });
                job.reset();
                vTaskDelete(nullptr);
            },
            "cyberdeck_join", 4096, job, 3, nullptr) != pdPASS) {
        delete job;
        busy_ = false;
        join_pending_ = false;
        SetText("Not enough memory. Try again.");
    }
}

std::string CyberdeckDisplay::ScanWifi() {
    wifi_scan_config_t config = {};
    config.show_hidden = true;
    config.scan_type = WIFI_SCAN_TYPE_PASSIVE;
    config.scan_time.passive = 120;
    extern esp_err_t CyberdeckSurveyWifi(const wifi_scan_config_t&, wifi_ap_record_t*, uint16_t&);
    std::array<wifi_ap_record_t, 24> records{};
    uint16_t count = records.size();
    auto error = CyberdeckSurveyWifi(config, records.data(), count);
    if (error != ESP_OK)
        return std::string("Scan unavailable: ") + esp_err_to_name(error) +
               "\nWait for Wi-Fi connection/setup to finish, then Refresh.";
    std::sort(records.begin(), records.begin() + count,
              [](const auto& a, const auto& b) { return a.rssi > b.rssi; });
    std::string text = "2.4GHz networks (up to 24)\nSaved Wi-Fi resumes automatically.\n\n";
    for (int i = 0; i < count; ++i) {
        char line[160];
        snprintf(line, sizeof(line), "%.*s\nChannel %u | %d dBm | %s\n\n", 32,
                 records[i].ssid[0] ? reinterpret_cast<const char*>(records[i].ssid) : "<hidden>",
                 records[i].primary, records[i].rssi,
                 records[i].authmode == WIFI_AUTH_OPEN ? "Open" : "Secured");
        text += line;
    }
    if (!count)
        text += "No networks found.";
    return text;
}

std::string CyberdeckDisplay::ScanBle() {
    if (!ble_started_) {
        ble_owner_ = this;
        if (nimble_port_init() != ESP_OK)
            return "BLE initialization failed.";
        ble_hs_cfg.sync_cb = []() { ble_owner_->ble_synced_ = true; };
        ble_hs_cfg.reset_cb = [](int) { ble_owner_->ble_synced_ = false; };
        nimble_port_freertos_init([](void*) {
            nimble_port_run();
            nimble_port_freertos_deinit();
        });
        ble_started_ = true;
    }
    for (int i = 0; i < 40 && !ble_synced_; ++i)
        vTaskDelay(pdMS_TO_TICKS(50));
    if (!ble_synced_)
        return "BLE radio is not ready. Try Refresh.";
    {
        std::lock_guard<std::mutex> lock(ble_mutex_);
        ble_devices_.clear();
    }
    ble_done_ = false;
    uint8_t address_type;
    if (ble_hs_id_infer_auto(0, &address_type))
        return "Could not select BLE address.";
    ble_gap_disc_params parameters = {};
    parameters.passive = 0;  // Request extra advertisement data, often including the name.
    parameters.filter_duplicates = 0;  // Merge advertisements and scan responses ourselves.
    int error = ble_gap_disc(
        address_type, 5000, &parameters,
        [](ble_gap_event* event, void* arg) -> int {
            auto self = static_cast<CyberdeckDisplay*>(arg);
            if (event->type == BLE_GAP_EVENT_DISC_COMPLETE)
                self->ble_done_ = true;
            if (event->type != BLE_GAP_EVENT_DISC)
                return 0;
            char address[20];
            auto& bytes = event->disc.addr.val;
            snprintf(address, sizeof(address), "%02X:%02X:%02X:%02X:%02X:%02X", bytes[5], bytes[4],
                     bytes[3], bytes[2], bytes[1], bytes[0]);
            ble_hs_adv_fields fields = {};
            std::string name;
            if (ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data) == 0 &&
                fields.name && fields.name_len) {
                name.assign(reinterpret_cast<const char*>(fields.name), fields.name_len);
            }
            std::lock_guard<std::mutex> lock(self->ble_mutex_);
            for (auto& device : self->ble_devices_) {
                if (device.address == address) {
                    device.rssi = event->disc.rssi;
                    cyberdeck::UpdateBleName(device.name, device.name_complete, name,
                                             fields.name_is_complete);
                    return 0;
                }
            }
            if (self->ble_devices_.size() < 24)
                self->ble_devices_.push_back(
                    {address, name, event->disc.rssi, static_cast<bool>(fields.name_is_complete)});
            return 0;
        },
        this);
    if (error)
        return "BLE scan could not start (radio busy).";
    for (int i = 0; i < 120 && !ble_done_; ++i)
        vTaskDelay(pdMS_TO_TICKS(50));
    if (!ble_done_)
        ble_gap_disc_cancel();
    std::lock_guard<std::mutex> lock(ble_mutex_);
    std::sort(ble_devices_.begin(), ble_devices_.end(),
              [](const auto& a, const auto& b) { return a.rssi > b.rssi; });
    auto named = std::count_if(ble_devices_.begin(), ble_devices_.end(),
                               [](const auto& device) { return !device.name.empty(); });
    ESP_LOGI("CyberdeckBLE", "Scan completed: %u devices, %u named",
             static_cast<unsigned>(ble_devices_.size()), static_cast<unsigned>(named));
    std::string text = "BLE active / 5-second scan\n\n";
    for (const auto& device : ble_devices_)
        text += (device.name.empty() ? "Unnamed" : device.name) + std::string("\n") +
                device.address + " | " + std::to_string(device.rssi) + " dBm\n\n";
    if (ble_devices_.empty())
        text += "No advertisements found.";
    return text;
}

std::string CyberdeckDisplay::FetchDashboard(Page page) {
    wifi_ap_record_t connected = {};
    if (esp_wifi_sta_get_ap_info(&connected) != ESP_OK)
        return "Internet needed.\nOpen Settings > Wi-Fi setup.";
    const char* url =
        page == Page::Weather || page == Page::Home
            ? "https://api.open-meteo.com/v1/forecast?latitude=" WEATHER_LATITUDE
              "&longitude=" WEATHER_LONGITUDE
              "&current=temperature_2m,relative_humidity_2m,wind_speed_10m,weather_code&timeformat="
              "unixtime"
            : "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin,ethereum,solana,dogecoin"
              "&vs_currencies=gbp&include_24hr_change=true";
    esp_http_client_config_t config = {};
    config.url = url;
    config.timeout_ms = 8000;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    auto client = esp_http_client_init(&config);
    if (!client)
        return "Could not create HTTPS request.";
    std::unique_ptr<std::remove_pointer_t<esp_http_client_handle_t>,
                    decltype(&esp_http_client_cleanup)>
        cleanup(client, esp_http_client_cleanup);
    if (esp_http_client_open(client, 0) != ESP_OK)
        return "Connection failed. Tap Refresh.";
    if (esp_http_client_fetch_headers(client) < 0)
        return "No response. Tap Refresh.";
    const int status = esp_http_client_get_status_code(client);
    if (status != 200)
        return "Service returned HTTP " + std::to_string(status) +
               ".\nTry again later (public API limits apply).";
    std::string response;
    char buffer[512];
    int length;
    const int64_t deadline = esp_timer_get_time() + 15000000;
    while ((length = esp_http_client_read(client, buffer, sizeof(buffer))) > 0) {
        if (esp_timer_get_time() > deadline)
            return "Response timed out. Tap Refresh.";
        if (response.size() + length > 16384)
            return "Response too large.";
        response.append(buffer, length);
    }
    if (length < 0 || !esp_http_client_is_complete_data_received(client))
        return "Incomplete response. Tap Refresh.";
    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> json(cJSON_Parse(response.c_str()),
                                                         cJSON_Delete);
    if (!json)
        return "Invalid data received.";
    std::string text;
    if (page == Page::Weather || page == Page::Home) {
        auto current = cJSON_GetObjectItemCaseSensitive(json.get(), "current");
        auto temperature = cJSON_GetObjectItemCaseSensitive(current, "temperature_2m");
        auto humidity = cJSON_GetObjectItemCaseSensitive(current, "relative_humidity_2m");
        auto wind = cJSON_GetObjectItemCaseSensitive(current, "wind_speed_10m");
        auto code = cJSON_GetObjectItemCaseSensitive(current, "weather_code");
        auto stamp = cJSON_GetObjectItemCaseSensitive(current, "time");
        if (!cJSON_IsNumber(temperature) || !std::isfinite(temperature->valuedouble) ||
            temperature->valuedouble < -100 || temperature->valuedouble > 100 ||
            !cJSON_IsNumber(humidity) || !std::isfinite(humidity->valuedouble) ||
            humidity->valuedouble < 0 || humidity->valuedouble > 100 || !cJSON_IsNumber(wind) ||
            !std::isfinite(wind->valuedouble) || wind->valuedouble < 0 || wind->valuedouble > 500 ||
            !cJSON_IsNumber(code) || code->valuedouble != code->valueint ||
            !cyberdeck::WeatherName(code->valueint) || !cJSON_IsNumber(stamp) ||
            !std::isfinite(stamp->valuedouble) || stamp->valuedouble < 1700000000 ||
            stamp->valuedouble > 2147483647 || stamp->valuedouble != std::floor(stamp->valuedouble))
            return "Weather response is missing readings.";
        const int temp = static_cast<int>(std::lround(temperature->valuedouble * 10));
        const int condition = code->valueint;
        const time_t measured = static_cast<time_t>(stamp->valuedouble);
        char cache[100];
        snprintf(cache, sizeof(cache), "{\"time\":%lld,\"temp\":%d,\"code\":%d}",
                 static_cast<long long>(measured), temp, condition);
        nvs_handle_t storage = 0;
        esp_err_t saved = nvs_open("cyberdeck", NVS_READWRITE, &storage);
        if (saved == ESP_OK) {
            saved = nvs_set_str(storage, WEATHER_CACHE_KEY, cache);
            if (saved == ESP_OK)
                saved = nvs_commit(storage);
            nvs_close(storage);
        }
        if (saved != ESP_OK)
            ESP_LOGW("CyberdeckHome", "Weather cache save failed: %s; keeping RAM reading",
                     esp_err_to_name(saved));
        // A completed reading remains useful even if the user has left the weather screen.
        Application::GetInstance().Schedule([this, temp, condition, measured]() {
            DisplayLockGuard lock(this);
            if (!lock)
                return;
            weather_temperature_ = temp;
            weather_code_ = condition;
            weather_time_ = measured;
            weather_updated_ = weather_attempt_ = lv_tick_get();
            weather_fetched_ = true;
            UpdateHome();
            ESP_LOGI("CyberdeckHome", "Weather ready: valid reading received");
        });
        char readings[150];
        snprintf(readings, sizeof(readings),
                 WEATHER_LOCATION "\n\nTemperature: %.1f C\nHumidity: %.0f%%\nWind: %.1f km/h",
                 temperature->valuedouble, humidity->valuedouble, wind->valuedouble);
        text = readings;
        text += "\n\nWeather: Open-Meteo";
    } else {
        const char* coins[] = {"bitcoin", "ethereum", "solana", "dogecoin"};
        text = "GBP / 24-hour change\n\n";
        for (const auto coin : coins) {
            auto entry = cJSON_GetObjectItemCaseSensitive(json.get(), coin);
            auto price = cJSON_GetObjectItemCaseSensitive(entry, "gbp");
            auto change = cJSON_GetObjectItemCaseSensitive(entry, "gbp_24h_change");
            char row[100];
            if (!cJSON_IsNumber(price) || !cJSON_IsNumber(change))
                snprintf(row, sizeof(row), "%s: unavailable\n", coin);
            else
                snprintf(row, sizeof(row), "%s\nGBP %.4f (%+.1f%%)\n\n", coin, price->valuedouble,
                         change->valuedouble);
            text += row;
        }
        text += "Prices: CoinGecko";
    }
    text += "\nFetched at uptime " + std::to_string(esp_timer_get_time() / 1000000) +
            "s.\nTap Refresh for new readings.";
    return text;
}
