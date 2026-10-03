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
    void AttachTouch(i2c_master_dev_handle_t touch);
    void Home();
    void ShowWifiSetup();

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
        WifiJoinStatus
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
    lv_obj_t* body_ = nullptr;
    lv_obj_t* title_ = nullptr;
    lv_obj_t* text_ = nullptr;
    lv_obj_t* voice_home_ = nullptr;
    lv_obj_t* voice_talk_ = nullptr;
    lv_obj_t* canvas_ = nullptr;
    uint16_t* pixels_ = nullptr;
    cyberdeck::Grid life_{};
    std::array<int, 20> rain_{};
    i2c_master_dev_handle_t touch_ = nullptr;
    std::atomic<bool> busy_{false};
    std::atomic<bool> ble_synced_{false};
    std::atomic<bool> ble_done_{false};
    std::mutex ble_mutex_;
    std::vector<BleDevice> ble_devices_;
    bool ble_started_ = false;
    bool time_sync_started_ = false;
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
