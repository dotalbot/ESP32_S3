#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

class CyberdeckRadio {
public:
    static CyberdeckRadio& Instance();
    void StartConsole();
    // Call control methods on Application's task; the worker never touches LVGL.
    void Start(int station);  // -1 tests the speaker; kStartupSound plays the boot chime.
    void Stop();
    bool IsRunning() const { return running_.load(); }
    std::string Status();
    void SetSleepMinutes(int minutes);
    int SleepMinutes() const { return sleep_minutes_; }
    static constexpr int kStationCount = 4;
    static constexpr int kStartupSound = -2;
    static const char* StationName(int station);

private:
    std::atomic<bool> running_{false};
    std::atomic<bool> cancelled_{false};
    std::atomic<int64_t> sleep_deadline_{0};
    std::atomic<int> sleep_minutes_{0};
    std::mutex mutex_;
    std::string status_ = "Choose a station, then Play.";
    uint32_t generation_ = 0;
    int station_ = 0;
    bool Cancelled();
    bool Queue(std::vector<int16_t>&& pcm);
    void SetStatus(const std::string& status);
    void Worker();
    std::string Stream();
};
