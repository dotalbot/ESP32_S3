# Pocket cyberdeck

Open [the board guide](main/boards/dominic-es3c28p-cyberdeck/README.md) for the menu, build command, hardware assumptions and first USB test.

The implementation is in `main/boards/dominic-es3c28p-cyberdeck/`. It combines Wi-Fi/BLE discovery with clock, weather, crypto, Matrix, Game of Life, XiaoZhi voice and device settings. The unique board profile is `dominic-es3c28p-cyberdeck`.

Validation: ESP-IDF 6.1 build passed for the English variant. Application image is 2,879,456 bytes with 30% free in the selected application partition. All 98 repository host checks and the board logic checks pass, including BLE names arriving after the initial advertisement.

The current build assumes 16 MB flash and octal PSRAM. Firmware images are in `build/`; `merged-binary.bin` includes the selected flash layout and assets. The original 16 MB firmware is backed up in `/Users/dominic/ESP32_Backups/`. The build has been flashed and its startup checked: PSRAM and display initialization passed, with no crash observed during the 12-second capture. The user confirmed visible rendering, touch calibration, Matrix → Home navigation and Game of Life. Wi-Fi/BLE discovery and internet apps have also been checked, as described below; audio and battery sensing still need physical checks. Touch startup now waits 300 ms after reset; calibration is saved in NVS. The final clean build uses 10 ms touch polling and activates Home on press; startup was rechecked after flashing.

Wi-Fi setup now scans networks and accepts the password on the touchscreen. Hotspot provisioning is disabled for this board. Credentials are saved for automatic reconnect.

The user confirmed Wi-Fi connection, Crypto, Weather and Clock working. BLE detects nearby devices. Its follow-up build uses active scanning and merges later scan responses so names can replace unnamed entries; complete names take priority over shortened ones. After flashing, a live scan logged 24 devices with 7 named. Devices without a supplied name still show Unnamed.

The Wi-Fi survey initially returned an empty list because the connection manager consumed the results. Diagnostics confirmed 16 detected networks followed by zero remaining records. The fix briefly pauses the station handler, copies passive scan results and resumes saved-network connection. Its hardware check returned 10 networks, reconnected and successfully fetched Weather. The final firmware includes this fix and removes the temporary automatic startup survey. The final connected-device check passed with 16 networks and a saved-network reconnect. To repeat it, run `python scripts/check_cyberdeck_wifi.py` in the IDF environment and tap Wi-Fi > Refresh.

The password field now has white characters on dark green. The inserted SD card mounted as 60,000 MB and the first settings backup passed a read-back check. Settings shows the SD status. Changed Wi-Fi credentials, brightness and touch calibration are exported within 10 seconds to `/cyberdeck/backup.json`, retaining `backup.bak`. Internal storage remains primary. This version exports backups without automatic import; absent/failed cards are optional. Restart after inserting a card. An 18-second startup capture after installation showed no crash. Removal/failure behaviour remains to be physically tested.
