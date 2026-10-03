#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>

#include <esp_wifi.h>
#include <ssid_manager.h>
#include <wifi_manager.h>
#include <wifi_station.h>
#include <mutex>
#include "adc_battery_monitor.h"
#include "application.h"
#include "button.h"
#include "codecs/es8311_audio_codec.h"
#include "config.h"
#include "cyberdeck_display.h"
#include "cyberdeck_storage.h"
#include "mcp_server.h"
#include "wifi_board.h"

#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "esp_lcd_ili9341.h"
#include "led/single_led.h"
#include "system_reset.h"

#define TAG "DominicCyberdeck"

class DominicCyberdeck : public WifiBoard {
private:
    std::mutex wifi_join_mutex_;
    Button boot_button_;
    CyberdeckDisplay* display_;
    i2c_master_bus_handle_t codec_i2c_bus_;
    i2c_master_dev_handle_t touch_ = nullptr;
    AdcBatteryMonitor* adc_battery_monitor_;

    void InitializeBatteryMonitor() {
        adc_battery_monitor_ =
            new AdcBatteryMonitor(ADC_UNIT_1, ADC_CHANNEL_8, 200000, 200000, GPIO_NUM_NC);
    }

    void InitializeTouch() {
        gpio_config_t reset = {};
        reset.pin_bit_mask = 1ULL << TOUCH_RESET_PIN;
        reset.mode = GPIO_MODE_OUTPUT;
        ESP_ERROR_CHECK(gpio_config(&reset));
        ESP_ERROR_CHECK(gpio_set_level(TOUCH_RESET_PIN, 0));
        vTaskDelay(pdMS_TO_TICKS(10));
        ESP_ERROR_CHECK(gpio_set_level(TOUCH_RESET_PIN, 1));
        // FT6336G needs this startup window before its first I2C transaction.
        vTaskDelay(pdMS_TO_TICKS(300));
        i2c_device_config_t device = {};
        device.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        device.device_address = 0x38;
        device.scl_speed_hz = 400000;
        ESP_ERROR_CHECK(i2c_master_bus_add_device(codec_i2c_bus_, &device, &touch_));
        display_->AttachTouch(touch_);
    }

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = AUDIO_CODEC_I2C_NUM,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags =
                {
                    .enable_internal_pullup = 1,
                },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = DISPLAY_MIS0_PIN;
        buscfg.sclk_io_num = DISPLAY_SCK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeButtons() {
        boot_button_.OnClick(
            [this]() { Application::GetInstance().Schedule([this]() { display_->Home(); }); });
        boot_button_.OnLongPress([this]() {
            Application::GetInstance().Schedule([this]() { display_->ShowWifiSetup(); });
        });
    }

    void InitializeLcdDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = DISPLAY_SPI_SCLK_HZ;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_SPI_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));
        ESP_LOGI(TAG, "Install LCD driver ILI9341");
        esp_lcd_panel_reset(panel);

        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        display_ = new CyberdeckDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                        DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                        DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

public:
    DominicCyberdeck() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeBatteryMonitor();
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeTouch();
        InitializeButtons();
        GetBacklight()->RestoreBrightness();
        CyberdeckStartStorage();
    }

    void StartNetwork() override {
        std::lock_guard<std::mutex> lock(wifi_join_mutex_);
        WifiBoard::StartNetwork();
        if (SsidManager::GetInstance().GetSsidList().empty()) {
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
            ESP_ERROR_CHECK(esp_wifi_start());
        }
    }

    bool BeginWifiJoin() {
        std::lock_guard<std::mutex> lock(wifi_join_mutex_);
        auto& wifi = WifiManager::GetInstance();
        if (!wifi.IsInitialized())
            return false;
        Application::GetInstance().ResetProtocol();
        Application::GetInstance().Schedule(
            []() { Application::GetInstance().SetDeviceState(kDeviceStateWifiConfiguring); });
        esp_timer_stop(connect_timer_);
        wifi.StopStation();
        esp_wifi_scan_stop();
        esp_wifi_stop();
        if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK)
            return false;
        return esp_wifi_start() == ESP_OK;
    }

    void ConnectWifi(const std::string& ssid, const std::string& password, uint8_t channel) {
        std::lock_guard<std::mutex> lock(wifi_join_mutex_);
        esp_wifi_scan_stop();
        esp_wifi_stop();
        auto& saved = SsidManager::GetInstance();
        saved.AddSsid(ssid, password, channel);
        const auto& networks = saved.GetSsidList();
        for (int i = 0; i < networks.size(); ++i) {
            if (networks[i].ssid == ssid) {
                saved.SetDefaultSsid(i);
                break;
            }
        }
        TryWifiConnect();
    }

    esp_err_t SurveyWifi(const wifi_scan_config_t& config, wifi_ap_record_t* records,
                         uint16_t& count) {
        std::lock_guard<std::mutex> lock(wifi_join_mutex_);
        auto& wifi = WifiManager::GetInstance();
        if (!records || !count)
            return ESP_ERR_INVALID_ARG;
        if (!wifi.IsInitialized())
            return ESP_ERR_WIFI_NOT_INIT;
        const bool reconnect = wifi.IsConnected();
        if (!reconnect && !SsidManager::GetInstance().GetSsidList().empty())
            return ESP_ERR_WIFI_STATE;
        // The connection manager consumes every scan-done event, even for a blocking scan.
        // Stop its handler while surveying, then restore normal saved-network connection.
        esp_timer_stop(connect_timer_);
        wifi.StopStation();
        esp_wifi_scan_stop();
        esp_wifi_stop();
        auto error = esp_wifi_set_mode(WIFI_MODE_STA);
        if (error == ESP_OK)
            error = esp_wifi_start();
        if (error == ESP_OK)
            error = esp_wifi_scan_start(&config, true);
        if (error == ESP_OK)
            error = esp_wifi_scan_get_ap_records(&count, records);
        if (error != ESP_OK)
            esp_wifi_clear_ap_list();
        esp_wifi_stop();
        if (reconnect)
            TryWifiConnect();
        else
            esp_wifi_start();
        if (error == ESP_OK)
            ESP_LOGI(TAG, "Survey complete: %u networks; reconnecting=%d", count, reconnect);
        else
            ESP_LOGW(TAG, "Survey failed: %s", esp_err_to_name(error));
        return error;
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            codec_i2c_bus_, AUDIO_CODEC_I2C_NUM, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN, AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR, true, true);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override { return display_; }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        charging = adc_battery_monitor_->IsCharging();
        discharging = adc_battery_monitor_->IsDischarging();
        level = adc_battery_monitor_->GetBatteryLevel();
        return true;
    }
};

DECLARE_BOARD(DominicCyberdeck);

bool CyberdeckBeginWifiJoin() {
    return static_cast<DominicCyberdeck&>(Board::GetInstance()).BeginWifiJoin();
}

void CyberdeckConnectWifi(const std::string& ssid, const std::string& password, uint8_t channel) {
    static_cast<DominicCyberdeck&>(Board::GetInstance()).ConnectWifi(ssid, password, channel);
}

esp_err_t CyberdeckSurveyWifi(const wifi_scan_config_t& config, wifi_ap_record_t* records,
                              uint16_t& count) {
    return static_cast<DominicCyberdeck&>(Board::GetInstance()).SurveyWifi(config, records, count);
}
