#include "cyberdeck_display.h"
#include <wifi_manager.h>
#include "application.h"
#include "board.h"
#include "config.h"
#include "cyberdeck_storage.h"
#include "settings.h"

#include <esp_crt_bundle.h>
#include <esp_heap_caps.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_netif_sntp.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <cJSON.h>
#include <host/ble_hs.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <type_traits>

LV_FONT_DECLARE(BUILTIN_TEXT_FONT);
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
    lv_obj_set_pos(title_, 10, 10);
    lv_obj_set_width(title_, 225);
    lv_label_set_long_mode(title_, LV_LABEL_LONG_DOT);
    Button(
        root_, "Home", 245, 4, 70, 34,
        [](lv_event_t* event) {
            static_cast<CyberdeckDisplay*>(lv_event_get_user_data(event))->Home();
        },
        this, LV_EVENT_PRESSED);
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
    last_touch_ = lv_tick_get();
    lv_timer_create(
        [](lv_timer_t* timer) {
            static_cast<CyberdeckDisplay*>(lv_timer_get_user_data(timer))->Tick();
        },
        100, this);
    Open(Page::Home);
    Settings touch_settings("cyberdeck");
    touch_swap_ = touch_settings.GetInt("touch_swap", TOUCH_SWAP_XY);
    touch_mirror_x_ = touch_settings.GetInt("touch_mx", TOUCH_MIRROR_X);
    touch_mirror_y_ = touch_settings.GetInt("touch_my", TOUCH_MIRROR_Y);
    if (!touch_settings.GetInt("touch_ready", 0))
        StartCalibration();
}

void CyberdeckDisplay::AttachTouch(i2c_master_dev_handle_t touch) {
    DisplayLockGuard lock(this);
    if (!lock)
        return;
    touch_ = touch;
    auto input = lv_indev_create();
    lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(input, display_);
    lv_indev_set_user_data(input, this);
    lv_indev_set_read_cb(input, ReadTouch);
    lv_timer_set_period(lv_indev_get_read_timer(input), 10);
}

void CyberdeckDisplay::ReadTouch(lv_indev_t* input, lv_indev_data_t* data) {
    auto self = static_cast<CyberdeckDisplay*>(lv_indev_get_user_data(input));
    data->state = LV_INDEV_STATE_RELEASED;
    uint8_t reg = 0x02;
    uint8_t buf[5] = {};
    const auto error = i2c_master_transmit_receive(self->touch_, &reg, 1, buf, sizeof(buf), 50);
    if (error != ESP_OK)
        return;
    if ((buf[0] & 0x0f) == 0) {
        self->ignore_touch_ = false;
        return;
    }
    self->last_touch_ = lv_tick_get();
    if (self->screen_off_) {
        self->screen_off_ = false;
        self->ignore_touch_ = true;
        Board::GetInstance().GetBacklight()->SetBrightness(self->brightness_);
    }
    if (self->ignore_touch_)
        return;
    const int raw_x = ((buf[1] & 0x0f) << 8) | buf[2];
    const int raw_y = ((buf[3] & 0x0f) << 8) | buf[4];
    if (self->calibration_step_ >= 0) {
        self->ignore_touch_ = true;
        self->CaptureCalibration(raw_x, raw_y);
        return;
    }
    auto [x, y] = cyberdeck::MapTouch(raw_x, raw_y, self->touch_swap_, self->touch_mirror_x_,
                                      self->touch_mirror_y_);
    if (x < 0)
        return;
    data->point.x = x;
    data->point.y = y;
    data->state = LV_INDEV_STATE_PRESSED;
}

void CyberdeckDisplay::Home() {
    DisplayLockGuard lock(this);
    if (!lock || !root_ || calibration_step_ >= 0)
        return;
    screen_off_ = false;
    last_touch_ = lv_tick_get();
    Board::GetInstance().GetBacklight()->SetBrightness(brightness_);
    Open(Page::Home);
}

void CyberdeckDisplay::ShowWifiSetup() {
    DisplayLockGuard lock(this);
    if (lock && root_ && calibration_step_ < 0)
        Open(Page::WifiSetup);
}

void CyberdeckDisplay::StartCalibration() {
    if (calibration_panel_)
        return;
    calibration_step_ = 0;
    ignore_touch_ = true;
    calibration_panel_ = lv_obj_create(lv_layer_top());
    lv_obj_set_size(calibration_panel_, 320, 240);
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
        auto orientation = cyberdeck::CalibrateTouch(calibration_points_);
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
    auto [next_x, next_y] = cyberdeck::kCalibrationTargets[calibration_step_];
    lv_obj_set_pos(calibration_target_, next_x - 12, next_y - 12);
}

void CyberdeckDisplay::SetText(const std::string& text) {
    if (text_) {
        lv_obj_set_style_text_color(text_, lv_color_hex(0xcaf7df), LV_PART_MAIN);
        lv_label_set_text(text_, text.c_str());
    }
}

void CyberdeckDisplay::Open(Page page) {
    ++request_;  // Invalidate results when the user leaves a screen.
    page_ = page;
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(voice_home_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(voice_talk_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clean(body_);
    lv_obj_scroll_to_y(body_, 0, LV_ANIM_OFF);
    text_ = nullptr;
    canvas_ = nullptr;
    password_input_ = nullptr;
    const char* names[] = {
        "Pocket Cyberdeck", "Wi-Fi survey",   "BLE discovery", "Clock", "Weather",
        "Crypto prices",    "Matrix",         "Game of Life",  "Voice", "Settings",
        "Wi-Fi setup",      "Wi-Fi password", "Joining Wi-Fi"};
    static_assert(std::size(names) == static_cast<size_t>(Page::WifiJoinStatus) + 1);
    lv_label_set_text(title_, names[static_cast<int>(page)]);
    if (page == Page::Home) {
        const char* apps[] = {"Wi-Fi",  "BLE",  "Clock", "Weather", "Crypto",
                              "Matrix", "Life", "Voice", "Settings"};
        for (int i = 0; i < 9; ++i) {
            auto button = Button(
                body_, apps[i], 7 + (i % 3) * 105, 4 + (i / 3) * 61, 96, 53,
                [](lv_event_t* event) {
                    auto self = static_cast<CyberdeckDisplay*>(lv_event_get_user_data(event));
                    auto page = static_cast<Page>(reinterpret_cast<intptr_t>(
                        lv_obj_get_user_data(lv_event_get_target_obj(event))));
                    self->Open(page);
                },
                this);
            lv_obj_set_user_data(button, reinterpret_cast<void*>(static_cast<intptr_t>(i + 1)));
        }
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
            pixels_ = static_cast<uint16_t*>(
                heap_caps_calloc(320 * 180, sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
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
    } else if (page == Page::Clock) {
        last_clock_ = 0;
        StartWork(Page::Clock);
    } else if (page == Page::Settings) {
        int level = 0;
        bool charging = false, discharging = false;
        Board::GetInstance().GetBatteryLevel(level, charging, discharging);
        char info[200];
        snprintf(info, sizeof(info), "Battery estimate: %d%%\nFree heap: %u KB\nSD: %s", level,
                 static_cast<unsigned>(esp_get_free_heap_size() / 1024), CyberdeckStorageStatus());
        SetText(info);
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

void CyberdeckDisplay::Tick() {
    const uint32_t now = lv_tick_get();
    if (!screen_off_ && now - last_touch_ >= 60000) {
        screen_off_ = true;
        Board::GetInstance().GetBacklight()->SetBrightness(0);
    }
    if (screen_off_)
        return;
    if (page_ == Page::WifiJoinStatus && !join_pending_ && now - last_clock_ >= 1000) {
        last_clock_ = now;
        auto& wifi = WifiManager::GetInstance();
        if (wifi.IsConnected()) {
            SetText("Connected to:\n" + wifi.GetSsid() + "\nIP: " + wifi.GetIpAddress() +
                    "\n\nHome > Clock checks time sync.");
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
        const time_t timestamp = time(nullptr);
        if (timestamp < 1700000000)
            return;
        struct tm local = {};
        localtime_r(&timestamp, &local);
        char clock[100];
        strftime(clock, sizeof(clock), "%H:%M:%S\n%A\n%d %B %Y\nEurope/London", &local);
        SetText(clock);
    }
    if (!canvas_ || now - last_frame_ < 150)
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

void CyberdeckDisplay::StartWork(Page page) {
    if (busy_.exchange(true)) {
        SetText("Another request is finishing.\nTap Refresh shortly.");
        return;
    }
    SetText(page == Page::WifiSetup ? "Scanning..."
            : page == Page::Wifi ? "Scanning...\nWi-Fi pauses briefly and reconnects automatically."
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
    if (page_ == page && request_ == request)
        SetText(result);
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
        page == Page::Weather
            ? "https://api.open-meteo.com/v1/forecast?latitude=" WEATHER_LATITUDE
              "&longitude=" WEATHER_LONGITUDE
              "&current=temperature_2m,relative_humidity_2m,wind_speed_10m"
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
    if (page == Page::Weather) {
        auto current = cJSON_GetObjectItemCaseSensitive(json.get(), "current");
        auto temperature = cJSON_GetObjectItemCaseSensitive(current, "temperature_2m");
        auto humidity = cJSON_GetObjectItemCaseSensitive(current, "relative_humidity_2m");
        auto wind = cJSON_GetObjectItemCaseSensitive(current, "wind_speed_10m");
        if (!cJSON_IsNumber(temperature) || !cJSON_IsNumber(humidity) || !cJSON_IsNumber(wind))
            return "Weather response is missing readings.";
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
