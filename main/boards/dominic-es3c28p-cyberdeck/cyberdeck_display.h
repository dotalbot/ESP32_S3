#pragma once

#include "cyberdeck_logic.h"
#include "display/lcd_display.h"

#include <driver/i2c_master.h>
#include <atomic>
#include <mutex>
#include <vector>

class CyberdeckDisplay : public SpiLcdDisplay {
public:
    using SpiLcdDisplay::SpiLcdDisplay;
    void SetupUI() override;
    void AttachTouch(i2c_master_dev_handle_t touch, int points = 1);
    void Home();
    void ShowWifiSetup();
    void ShowStartupDuck();
    bool CheckRadioScreen(bool open);
    bool CheckHomeScreen(bool open);

private:
    enum class Page {
        Home,
        Wifi,
        Ble,
        Clock,
        Weather,
        Crypto,
        Matrix,
        Life,
        Voice,
        Settings,
        WifiSetup,
        WifiPassword,
        WifiJoinStatus,
        Radio,
        Apps,
        SetTime
    };
    struct JoinNetwork {
        std::string ssid;
        uint8_t channel;
        int rssi;
        bool open;
        bool supported;
    };
    std::vector<JoinNetwork> join_networks_;
    JoinNetwork selected_network_{};
    lv_obj_t* password_input_ = nullptr;
    lv_obj_t* radio_status_ = nullptr;
    lv_obj_t* radio_volume_ = nullptr;
    int radio_station_ = 0;
    void ShowRadio();
    bool join_pending_ = false;
    uint32_t join_started_ = 0;
    void ScanJoinNetworks(uint64_t request);
    void ShowJoinNetworks();
    void ChooseNetwork(size_t index);
    void ConnectSelected(std::string password);
    struct BleDevice {
        std::string address;
        std::string name;
        int rssi;
        bool name_complete = false;
    };
    Page page_ = Page::Home;
    lv_obj_t* root_ = nullptr;
    lv_obj_t* startup_panel_ = nullptr;
    lv_obj_t* startup_duck_ = nullptr;
    lv_obj_t* startup_shadow_ = nullptr;
    uint32_t startup_started_ = 0;
    lv_obj_t* body_ = nullptr;
    lv_obj_t* title_ = nullptr;
    lv_obj_t* header_home_ = nullptr;
    lv_obj_t* wifi_status_ = nullptr;
    lv_obj_t* battery_status_ = nullptr;
    lv_obj_t* home_temperature_ = nullptr;
    lv_obj_t* home_condition_ = nullptr;
    lv_obj_t* home_weather_time_ = nullptr;
    lv_obj_t* home_date_ = nullptr;
    lv_obj_t* home_set_time_ = nullptr;
    std::array<lv_obj_t*, 3> home_buttons_{};
    std::array<lv_obj_t*, 5> time_rollers_{};
    void ShowHome();
    void UpdateHome();
    void UpdateStatus();
    void ShowSetTime();
    int weather_temperature_ = 0;
    int weather_code_ = -1;
    time_t weather_time_ = 0;
    bool weather_fetched_ = false;
    bool was_connected_ = false;
    uint32_t weather_updated_ = 0;
    uint32_t weather_attempt_ = 0;
    uint32_t last_status_ = 0;
    lv_obj_t* text_ = nullptr;
    lv_obj_t* voice_home_ = nullptr;
    lv_obj_t* voice_talk_ = nullptr;
    lv_obj_t* canvas_ = nullptr;
    uint16_t* pixels_ = nullptr;
    int clock_style_ = 0;
    lv_obj_t* clock_default_label_ = nullptr;
    void ShowClock();
    void UpdateClock();
    cyberdeck::Grid life_{};
    std::array<int, 20> rain_{};
    i2c_master_dev_handle_t touch_ = nullptr;
    int touch_points_ = 1;
    bool body_scaled_ = false;
    void LayoutBody();
    std::atomic<bool> busy_{false};
    std::atomic<bool> ble_synced_{false};
    std::atomic<bool> ble_done_{false};
    std::mutex ble_mutex_;
    std::vector<BleDevice> ble_devices_;
    bool ble_started_ = false;
    bool time_sync_started_ = false;
    std::atomic<bool> time_sync_restart_{false};
    bool touch_swap_ = true;
    bool touch_mirror_x_ = false;
    bool touch_mirror_y_ = true;
    int calibration_step_ = -1;
    std::array<cyberdeck::TouchPoint, 3> calibration_points_{};
    lv_obj_t* calibration_panel_ = nullptr;
    lv_obj_t* calibration_target_ = nullptr;
    lv_obj_t* calibration_text_ = nullptr;
    bool screen_off_ = false;
    bool ignore_touch_ = false;
    int brightness_ = 60;
    uint32_t last_touch_ = 0;
    uint32_t last_frame_ = 0;
    uint32_t last_clock_ = 0;
    uint64_t request_ = 0;
    std::string last_wifi_ = "No scan yet.";
    static CyberdeckDisplay* ble_owner_;

    void StartCalibration();
    void CaptureCalibration(int x, int y);
    void Open(Page page);
    void Tick();
    void StartWork(Page page);
    void RunWork(Page page, uint64_t request);
    std::string ScanWifi();
    std::string ScanBle();
    std::string FetchDashboard(Page page);
    void Finish(Page page, uint64_t request, std::string result);
    void SetText(const std::string& text);
    lv_obj_t* Button(lv_obj_t* parent, const char* label, int x, int y, int w, int h,
                     lv_event_cb_t callback, void* data,
                     lv_event_code_t trigger = LV_EVENT_CLICKED);
    static void ReadTouch(lv_indev_t* input, lv_indev_data_t* data);
};
