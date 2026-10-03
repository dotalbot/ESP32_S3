#include "cyberdeck_storage.h"

#include <sys/stat.h>
#include <unistd.h>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include <driver/sdmmc_host.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_vfs_fat.h>
#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs.h>

#include "settings.h"

namespace {
constexpr const char* kDirectory = "/sdcard/cyberdeck";
constexpr const char* kBackup = "/sdcard/cyberdeck/backup.json";
constexpr const char* kPrevious = "/sdcard/cyberdeck/backup.bak";
constexpr const char* kTemporary = "/sdcard/cyberdeck/backup.tmp";
constexpr const char* kTag = "CyberdeckSD";
std::atomic<const char*> status{"checking"};
using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;

// Read committed NVS keys rather than sharing the Wi-Fi manager's mutable vector.
void AddNamespace(cJSON* root, const char* name) {
    auto object = cJSON_AddObjectToObject(root, name);
    if (!object)
        return;
    nvs_handle_t handle;
    if (nvs_open(name, NVS_READONLY, &handle) != ESP_OK)
        return;
    nvs_iterator_t iterator = nullptr;
    auto error = nvs_entry_find("nvs", name, NVS_TYPE_ANY, &iterator);
    while (error == ESP_OK) {
        nvs_entry_info_t entry;
        nvs_entry_info(iterator, &entry);
        if (entry.type == NVS_TYPE_STR) {
            size_t size = 0;
            if (nvs_get_str(handle, entry.key, nullptr, &size) == ESP_OK && size <= 256) {
                std::string value(size, '\0');
                if (nvs_get_str(handle, entry.key, value.data(), &size) == ESP_OK)
                    cJSON_AddStringToObject(object, entry.key, value.c_str());
            }
        } else if (entry.type == NVS_TYPE_I32) {
            int32_t value;
            if (nvs_get_i32(handle, entry.key, &value) == ESP_OK)
                cJSON_AddNumberToObject(object, entry.key, value);
        } else if (entry.type == NVS_TYPE_U8) {
            uint8_t value;
            if (nvs_get_u8(handle, entry.key, &value) == ESP_OK)
                cJSON_AddNumberToObject(object, entry.key, value);
        }
        error = nvs_entry_next(&iterator);
    }
    nvs_release_iterator(iterator);
    nvs_close(handle);
}

std::string Snapshot() {
    Json root(cJSON_CreateObject(), cJSON_Delete);
    if (!root)
        return {};
    uint8_t mac[6];
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK)
        return {};
    char device[18];
    snprintf(device, sizeof(device), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2],
             mac[3], mac[4], mac[5]);
    cJSON_AddNumberToObject(root.get(), "version", 1);
    cJSON_AddStringToObject(root.get(), "device", device);
    AddNamespace(root.get(), "wifi");
    AddNamespace(root.get(), "display");
    AddNamespace(root.get(), "cyberdeck");
    std::unique_ptr<char, decltype(&cJSON_free)> encoded(cJSON_PrintUnformatted(root.get()),
                                                         cJSON_free);
    return encoded ? std::string(encoded.get()) : std::string{};
}

bool Save(const std::string& snapshot) {
    auto file = fopen(kTemporary, "wb");
    if (!file)
        return false;
    bool ok = fwrite(snapshot.data(), 1, snapshot.size(), file) == snapshot.size();
    if (fflush(file) != 0 || fsync(fileno(file)) != 0)
        ok = false;
    if (fclose(file) != 0)
        ok = false;
    if (!ok)
        return false;

    // Read back the complete temporary file before replacing a good backup.
    file = fopen(kTemporary, "rb");
    if (!file)
        return false;
    std::string verified(snapshot.size(), '\0');
    ok = fread(verified.data(), 1, verified.size(), file) == verified.size();
    if (fclose(file) != 0 || !ok || verified != snapshot)
        return false;
    if (access(kBackup, F_OK) == 0) {
        if (unlink(kPrevious) != 0 && errno != ENOENT)
            return false;
        if (rename(kBackup, kPrevious) != 0)
            return false;
    } else if (errno != ENOENT) {
        return false;
    }
    return rename(kTemporary, kBackup) == 0;
}

void StorageTask(void*) {
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;
    host.command_timeout_ms = 1000;
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    // LCDwiki ES3C28P SDIO wiring, separate from the display SPI bus.
    slot.width = 4;
    slot.clk = GPIO_NUM_38;
    slot.cmd = GPIO_NUM_40;
    slot.d0 = GPIO_NUM_39;
    slot.d1 = GPIO_NUM_41;
    slot.d2 = GPIO_NUM_48;
    slot.d3 = GPIO_NUM_47;
    esp_vfs_fat_mount_config_t mount = {};
    {
        Settings settings("cyberdeck", true);
        // One-time authorisation for the inserted card, never reformat after a later failure.
        mount.format_if_mount_failed = settings.GetBool("sd_init_format", true);
        settings.SetBool("sd_init_format", false);
    }
    mount.max_files = 3;
    mount.allocation_unit_size = 16 * 1024;
    mount.disk_status_check_enable = true;
    sdmmc_card_t* card = nullptr;
    auto error = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot, &mount, &card);
    if (error != ESP_OK) {
        ESP_LOGW(kTag, "Card unavailable (%s); continuing with internal storage",
                 esp_err_to_name(error));
        status = "unavailable; internal storage active";
        vTaskDelete(nullptr);
        return;
    }
    if (mkdir(kDirectory, 0700) != 0 && errno != EEXIST) {
        status = "write failed; internal storage active";
        esp_vfs_fat_sdcard_unmount("/sdcard", card);
        vTaskDelete(nullptr);
        return;
    }
    ESP_LOGI(kTag, "Card mounted: %llu MB",
             static_cast<unsigned long long>(static_cast<uint64_t>(card->csd.capacity) *
                                             card->csd.sector_size / (1024 * 1024)));
    {
        std::string previous;
        while (true) {
            auto snapshot = Snapshot();
            if (!snapshot.empty() && snapshot != previous) {
                if (!Save(snapshot)) {
                    ESP_LOGW(kTag, "Backup write failed; continuing with internal storage");
                    status = "write failed; internal storage active";
                    break;
                }
                previous = std::move(snapshot);
                status = "backup saved";
                ESP_LOGI(kTag, "Backup written and read-back verified");
            }
            // ponytail: poll for changed NVS settings; use notifications if more writers are added.
            vTaskDelay(pdMS_TO_TICKS(10000));
        }
    }
    esp_vfs_fat_sdcard_unmount("/sdcard", card);
    vTaskDelete(nullptr);
}
}  // namespace

void CyberdeckStartStorage() {
    if (xTaskCreate(StorageTask, "cyberdeck_sd", 6144, nullptr, 1, nullptr) != pdPASS)
        status = "unavailable; internal storage active";
}

const char* CyberdeckStorageStatus() { return status.load(); }
