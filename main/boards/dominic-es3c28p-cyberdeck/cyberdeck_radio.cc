#include "cyberdeck_radio.h"
#include "application.h"
#include "board.h"
#include "cyberdeck_config.h"
#include "cyberdeck_display.h"
#include "settings.h"

#include <esp_ae_rate_cvt.h>
#include <esp_audio_simple_dec.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_mp3_dec.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <strings.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

namespace {
constexpr const char* kNames[] = {"Groove Salad", "Drone Zone", "Secret Agent",
                                  "BBC World Service"};
// ponytail: fixed direct MP3 presets; resolve broadcaster playlists if servers change.
constexpr const char* kUrls[] = {"https://ice5.somafm.com/groovesalad-128-mp3",
                                 "https://ice5.somafm.com/dronezone-128-mp3",
                                 "https://ice5.somafm.com/secretagent-128-mp3",
                                 "https://stream.live.vc.bbcmedia.co.uk/bbc_world_service"};
static_assert(std::size(kNames) == CyberdeckRadio::kStationCount);
static_assert(std::size(kUrls) == CyberdeckRadio::kStationCount);
constexpr const char* kTag = "CyberdeckRadio";
}  // namespace

CyberdeckRadio& CyberdeckRadio::Instance() {
    static CyberdeckRadio radio;
    return radio;
}

void CyberdeckRadio::StartConsole() {
    // USB-only hardware check controls; exact, bounded commands, no network endpoint.
    if (xTaskCreate(
            [](void*) {
                // This board uses USB as a secondary log sink; stdin still points to UART0.
                FILE* input = std::fopen("/dev/secondary", "r");
                if (!input) {
                    ESP_LOGW(kTag, "USB check input unavailable; use touchscreen controls.");
                    vTaskDelete(nullptr);
                    return;
                }
                char line[64];
                bool discard = false;
                size_t length = 0;
                while (true) {
                    int ch = std::fgetc(input);
                    if (ch == EOF) {
                        clearerr(input);
                        vTaskDelay(pdMS_TO_TICKS(20));
                        continue;
                    }
                    if (ch == '\r')
                        continue;
                    if (ch != '\n') {
                        if (length < sizeof(line) - 1)
                            line[length++] = ch;
                        else
                            discard = true;
                        continue;
                    }
                    line[length] = 0;
                    if (!discard) {
                        std::string command(line);
                        Application::GetInstance().Schedule([command]() {
                            auto& radio = Instance();
                            if (command == "radio reboot") {
                                Application::GetInstance().Reboot();
                            } else if (command == "home screen" ||
                                       command == "home screen status") {
                                auto display = static_cast<CyberdeckDisplay*>(
                                    Board::GetInstance().GetDisplay());
                                if (!display->CheckHomeScreen(command == "home screen"))
                                    ESP_LOGE("CyberdeckHome", "Home screen missing");
                            } else if (command == "weather screen" || command == "weather status") {
                                auto display = static_cast<CyberdeckDisplay*>(
                                    Board::GetInstance().GetDisplay());
                                if (!display->CheckWeatherScreen(command == "weather screen"))
                                    ESP_LOGE("CyberdeckHome",
                                             "Weather screen missing, overflowing or unreadable");
                            } else if (command == "display off") {
                                static_cast<CyberdeckDisplay*>(Board::GetInstance().GetDisplay())
                                    ->DisplayOff();
                            } else if (command == "wifi reconnect") {
                                const auto result = esp_wifi_disconnect();
                                ESP_LOGI("CyberdeckHome", "Testing saved-network reconnect: %s",
                                         esp_err_to_name(result));
                            } else if (command == "radio screen" ||
                                       command == "radio screen status") {
                                auto display = static_cast<CyberdeckDisplay*>(
                                    Board::GetInstance().GetDisplay());
                                ESP_LOGI(kTag, "Radio screen %s",
                                         display->CheckRadioScreen(command == "radio screen")
                                             ? "ready"
                                             : "missing");
                            } else if (command == "radio test") {
                                Board::GetInstance().GetAudioCodec()->SetOutputVolume(20);
                                radio.Start(-1);
                            } else if (command == "radio chime") {
                                radio.Start(kStartupSound);
                            } else if (command == "radio stop") {
                                radio.Stop();
                            } else if (command.size() == 12 &&
                                       command.compare(0, 11, "radio play ") == 0 &&
                                       command[11] >= '0' && command[11] < '0' + kStationCount) {
                                radio.Start(command[11] - '0');
                            }
                        });
                    }
                    length = 0;
                    discard = false;
                }
            },
            "cyberdeck_audio_check", 3072, nullptr, 2, nullptr) != pdPASS)
        ESP_LOGW(kTag, "USB audio checks unavailable; use touchscreen controls.");
}

const char* CyberdeckRadio::StationName(int station) {
    return station >= 0 && station < kStationCount ? kNames[station] : "Speaker test";
}

void CyberdeckRadio::SetStatus(const std::string& status) {
    std::lock_guard<std::mutex> lock(mutex_);
    status_ = status;
    ESP_LOGI(kTag, "%s", status.c_str());
}

std::string CyberdeckRadio::Status() {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

void CyberdeckRadio::Start(int station) {
    auto& app = Application::GetInstance();
    if (station < kStartupSound || station >= kStationCount)
        return;
    if (running_) {
        SetStatus("Press Stop; wait a moment before changing station.");
        return;
    }
    const bool configuring =
        station == kStartupSound && app.GetDeviceState() == kDeviceStateWifiConfiguring;
    if (app.GetDeviceState() != kDeviceStateIdle && !configuring) {
        SetStatus("Voice/device is busy. Try Play when it finishes.");
        return;
    }
    wifi_ap_record_t connection{};
    if (station >= 0 && esp_wifi_sta_get_ap_info(&connection) != ESP_OK) {
        SetStatus("Internet needed: Settings > Wi-Fi setup.");
        return;
    }
    if (!configuring && !app.SetDeviceState(kDeviceStateNotifying)) {
        SetStatus("Audio is busy. Try again.");
        return;
    }
    station_ = station;
    cancelled_ = false;
    running_ = true;
    sleep_deadline_ = station >= 0 && sleep_minutes_
                          ? esp_timer_get_time() + int64_t(sleep_minutes_) * 60000000
                          : 0;
    app.GetAudioService().ResetDecoder();
    generation_ = app.GetAudioService().PlaybackGeneration();
    Board::GetInstance().SetPowerSaveLevel(PowerSaveLevel::PERFORMANCE);
    SetStatus(station == kStartupSound ? "Playing startup chime..."
              : station < 0            ? "Playing two quiet test tones..."
                                       : "Connecting...");
    if (station >= 0) {
        Settings settings("cyberdeck", true);
        settings.SetInt("radio_station", station);
    }
    if (xTaskCreate(
            [](void* arg) {
                static_cast<CyberdeckRadio*>(arg)->Worker();
                vTaskDelete(nullptr);
            },
            "cyberdeck_radio", 8192, this, 4, nullptr) != pdPASS) {
        running_ = false;
        if (!configuring)
            app.SetDeviceState(kDeviceStateIdle);
        Board::GetInstance().SetPowerSaveLevel(PowerSaveLevel::LOW_POWER);
        SetStatus("Not enough memory. Try again.");
    }
}

void CyberdeckRadio::Stop() {
    if (!running_)
        return;
    cancelled_ = true;
    sleep_deadline_ = 0;
    sleep_minutes_ = 0;
    auto& app = Application::GetInstance();
    auto& audio = app.GetAudioService();
    if (generation_ == audio.PlaybackGeneration()) {
        audio.ResetDecoder();
        if (app.GetDeviceState() == kDeviceStateNotifying)
            app.SetDeviceState(kDeviceStateIdle);
        Board::GetInstance().SetPowerSaveLevel(PowerSaveLevel::LOW_POWER);
    }
    SetStatus("Stopped. Closing connection...");
}

void CyberdeckRadio::SetSleepMinutes(int minutes) {
    if (minutes != 0 && minutes != 15 && minutes != 30 && minutes != 60)
        return;
    sleep_minutes_ = minutes;
    sleep_deadline_ = running_ && minutes ? esp_timer_get_time() + int64_t(minutes) * 60000000 : 0;
}

bool CyberdeckRadio::Cancelled() {
    auto& app = Application::GetInstance();
    const auto state = app.GetDeviceState();
    const bool startup_state = station_ == kStartupSound &&
                               (state == kDeviceStateIdle || state == kDeviceStateWifiConfiguring);
    return cancelled_ || (state != kDeviceStateNotifying && !startup_state) ||
           generation_ != app.GetAudioService().PlaybackGeneration() ||
           (sleep_deadline_ && esp_timer_get_time() >= sleep_deadline_);
}

bool CyberdeckRadio::Queue(std::vector<int16_t>&& pcm) {
    auto& audio = Application::GetInstance().GetAudioService();
    for (int retry = 0; retry < 40 && !Cancelled(); ++retry)
        if (audio.PushPcmToPlaybackQueue(std::move(pcm), generation_))
            return true;
    return false;
}

void CyberdeckRadio::Worker() {
    std::string result;
    if (station_ < 0) {
        const bool startup = station_ == kStartupSound;
        bool queued_all = true;
        for (int frame = 0; frame < (startup ? 30 : 40) && !Cancelled(); ++frame) {
            std::vector<int16_t> pcm(600);
            for (size_t i = 0; i < pcm.size(); ++i) {
                const int sample = frame * 600 + i;
                if (startup) {
                    // Four soft ascending bell notes, 750 ms total. Honour saved volume/mute.
                    constexpr float notes[] = {523.25f, 659.25f, 783.99f, 1046.50f};
                    const int note = std::min(sample / 3600, 3);
                    const int offset = sample - note * 3600;
                    const int duration = note == 3 ? 7200 : 3600;
                    const float envelope = std::min(1.0f, offset / 120.0f) *
                                           std::min(1.0f, (duration - 1 - offset) / 480.0f) *
                                           std::exp(-3.0f * offset / duration);
                    const float phase = 6.2831853f * notes[note] * offset / 24000;
                    pcm[i] = static_cast<int16_t>(1800 * envelope *
                                                  (std::sin(phase) + 0.18f * std::sin(2 * phase)));
                } else {
                    const float frequency = frame < 20 ? 440.0f : 660.0f;
                    // Gentle attack/release; use normal saved speaker volume.
                    float envelope =
                        std::min(1.0f, std::min(sample % 12000, 11999 - sample % 12000) / 240.0f);
                    pcm[i] = static_cast<int16_t>(
                        2000 * envelope * std::sin(6.2831853f * frequency * sample / 24000));
                }
            }
            if (!Queue(std::move(pcm))) {
                queued_all = false;
                break;
            }
        }
        while (!Cancelled() && !Application::GetInstance().GetAudioService().IsPlaybackIdle())
            vTaskDelay(pdMS_TO_TICKS(20));
        result = queued_all ? (startup ? "Startup chime complete."
                                       : "Speaker test complete. Choose a station and Play.")
                            : "Speaker test failed: audio queue stalled.";
    } else {
        for (int attempt = 0; attempt < 3 && !Cancelled(); ++attempt) {
            result = Stream();
            if (Cancelled())
                break;
            SetStatus(result + (attempt < 2 ? " Retrying..." : " Tap Play to retry."));
            for (int i = 0; i < 20 && !Cancelled(); ++i)
                vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    const bool stopped = Cancelled();
    Application::GetInstance().Schedule([this, result, stopped]() {
        auto& app = Application::GetInstance();
        if (generation_ == app.GetAudioService().PlaybackGeneration()) {
            app.GetAudioService().ResetDecoder();
            if (app.GetDeviceState() == kDeviceStateNotifying)
                app.SetDeviceState(kDeviceStateIdle);
            Board::GetInstance().SetPowerSaveLevel(PowerSaveLevel::LOW_POWER);
        }
        sleep_deadline_ = 0;
        sleep_minutes_ = 0;
        running_ = false;
        SetStatus(stopped ? "Stopped." : result);
    });
}

std::string CyberdeckRadio::Stream() {
    bool requires_metadata = false;
    esp_http_client_config_t config{};
    config.url = kUrls[station_];
    config.timeout_ms = 3000;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.buffer_size = 8192;
    config.user_data = &requires_metadata;
    config.event_handler = [](esp_http_client_event_t* event) -> esp_err_t {
        if (event->event_id == HTTP_EVENT_ON_HEADER && event->header_key && event->header_value &&
            strcasecmp(event->header_key, "icy-metaint") == 0)
            *static_cast<bool*>(event->user_data) =
                std::strtoul(event->header_value, nullptr, 10) > 0;
        return ESP_OK;
    };
    auto client = esp_http_client_init(&config);
    if (!client)
        return "Cannot open stream.";
    std::unique_ptr<std::remove_pointer_t<esp_http_client_handle_t>,
                    decltype(&esp_http_client_cleanup)>
        cleanup(client, esp_http_client_cleanup);
    esp_http_client_set_header(client, "Icy-MetaData", "0");
    esp_http_client_set_header(client, "User-Agent", "PocketCyberdeck/1.0");
    if (esp_http_client_open(client, 0) != ESP_OK || esp_http_client_fetch_headers(client) < 0)
        return "Stream connection failed.";
    const int status = esp_http_client_get_status_code(client);
    if (status != 200)
        return "Station returned HTTP " + std::to_string(status) + ".";
    if (requires_metadata)
        return "Station requires unsupported metadata.";

    static bool registered = false;  // Only this single worker uses the decoder registry.
    if (!registered) {
        if (esp_mp3_dec_register() != ESP_AUDIO_ERR_OK)
            return "Decoder registration failed.";
        registered = true;
    }
    esp_audio_simple_dec_cfg_t decoder_config{};
    decoder_config.dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_MP3;
    esp_audio_simple_dec_handle_t decoder = nullptr;
    if (esp_audio_simple_dec_open(&decoder_config, &decoder) != ESP_AUDIO_ERR_OK)
        return "Cannot start MP3 decoder.";
    std::unique_ptr<void, decltype(&esp_audio_simple_dec_close)> close_decoder(
        decoder, esp_audio_simple_dec_close);
    esp_ae_rate_cvt_handle_t converter = nullptr;
    std::unique_ptr<void, decltype(&esp_ae_rate_cvt_close)> close_converter(nullptr,
                                                                            esp_ae_rate_cvt_close);
    uint32_t source_rate = 0;
    std::vector<uint8_t> input(8192), output(8192);
    uint64_t samples = 0;
    int64_t report = esp_timer_get_time();
    while (!Cancelled()) {
        int received =
            esp_http_client_read(client, reinterpret_cast<char*>(input.data()), input.size());
        if (received <= 0)
            return "Stream ended or timed out.";
        esp_audio_simple_dec_raw_t raw{};
        raw.buffer = input.data();
        raw.len = received;
        while (raw.len && !Cancelled()) {
            esp_audio_simple_dec_out_t frame{};
            frame.buffer = output.data();
            frame.len = output.size();
            auto error = esp_audio_simple_dec_process(decoder, &raw, &frame);
            if (error == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH && frame.needed_size > output.size() &&
                frame.needed_size <= 32768) {
                output.resize(frame.needed_size);
                continue;
            }
            if (error != ESP_AUDIO_ERR_OK || raw.consumed > raw.len)
                return "MP3 decode failed.";
            if (frame.decoded_size) {
                esp_audio_simple_dec_info_t info{};
                if (esp_audio_simple_dec_get_info(decoder, &info) != ESP_AUDIO_ERR_OK ||
                    info.bits_per_sample != 16 || info.channel < 1 || info.channel > 2 ||
                    info.sample_rate < 8000 || info.sample_rate > 96000 ||
                    frame.decoded_size > output.size() || frame.decoded_size % (2 * info.channel))
                    return "Unsupported audio format.";
                const auto count = frame.decoded_size / (2 * info.channel);
                std::vector<int16_t> mono(count);
                const auto decoded = reinterpret_cast<const int16_t*>(output.data());
                for (size_t i = 0; i < count; ++i)
                    mono[i] = info.channel == 1
                                  ? decoded[i]
                                  : (int32_t(decoded[2 * i]) + decoded[2 * i + 1]) / 2;
                if (source_rate != info.sample_rate) {
                    close_converter.reset();
                    converter = nullptr;
                    source_rate = info.sample_rate;
                    if (source_rate != AUDIO_OUTPUT_SAMPLE_RATE) {
                        esp_ae_rate_cvt_cfg_t rate{};
                        rate.src_rate = source_rate;
                        rate.dest_rate = AUDIO_OUTPUT_SAMPLE_RATE;
                        rate.channel = 1;
                        rate.bits_per_sample = 16;
                        rate.complexity = 2;
                        rate.perf_type = ESP_AE_RATE_CVT_PERF_TYPE_SPEED;
                        if (esp_ae_rate_cvt_open(&rate, &converter) != ESP_AE_ERR_OK)
                            return "Audio conversion failed.";
                        close_converter.reset(converter);
                    }
                    ESP_LOGI(kTag, "MP3 %lu Hz, %u channels -> %d Hz mono",
                             static_cast<unsigned long>(source_rate), info.channel,
                             AUDIO_OUTPUT_SAMPLE_RATE);
                }
                if (converter) {
                    uint32_t capacity = 0;
                    if (esp_ae_rate_cvt_get_max_out_sample_num(converter, count, &capacity) !=
                            ESP_AE_ERR_OK ||
                        capacity > 4800)
                        return "Audio frame too large.";
                    std::vector<int16_t> converted(capacity);
                    if (esp_ae_rate_cvt_process(converter, mono.data(), count, converted.data(),
                                                &capacity) != ESP_AE_ERR_OK)
                        return "Audio conversion failed.";
                    converted.resize(capacity);
                    mono = std::move(converted);
                }
                if (!mono.empty()) {
                    const auto size = mono.size();
                    if (!Queue(std::move(mono)))
                        return "Playback interrupted.";
                    if (samples == 0)
                        SetStatus(std::string("Playing: ") + StationName(station_));
                    samples += size;
                }
                if (esp_timer_get_time() - report >= 10000000) {
                    ESP_LOGI(kTag, "Playback verified: %llu PCM samples queued",
                             static_cast<unsigned long long>(samples));
                    report = esp_timer_get_time();
                }
            }
            if (!raw.consumed && !frame.decoded_size)
                return "MP3 parser stalled.";
            raw.buffer += raw.consumed;
            raw.len -= raw.consumed;
            raw.consumed = 0;
        }
    }
    return "Stopped.";
}
