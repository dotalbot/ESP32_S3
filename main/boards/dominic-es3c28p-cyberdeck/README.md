# Dominic's portable 2.8-inch cyberdeck

A touchscreen launcher built into XiaoZhi, combining the five-app CYD idea with wireless discovery. Select the unique `dominic-es3c28p-cyberdeck` variant; the existing Freenove variant stays separate.

## Menu

| App | Behaviour | Connection |
|---|---|---|
| Wi-Fi | Passive 2.4GHz survey: name, channel, signal and security; up to 24 results | Briefly pauses saved-network connection, then reconnects automatically; wait if already connecting |
| BLE | Active five-second scan: name, address and signal; merges scan responses; up to 24 results | No internet |
| Clock | London time with UK daylight saving; automatic NTP sync online; manual date/time offline | Internet initially; runs locally afterwards |
| Weather | London temperature, humidity and wind from Open-Meteo | Internet; manual refresh |
| Crypto | Bitcoin, Ethereum, Solana and Dogecoin in GBP with 24-hour changes | Internet; manual refresh; public API limits apply |
| Matrix | Green pixel-glyph falling-rain animation | Offline |
| Life | Conway's Game of Life on a wrapping 32×18 grid; reopen to reseed | Offline |
| Voice | Stock XiaoZhi screen; tap Talk / stop to interact | Wi-Fi, XiaoZhi activation/server and audio hardware |
| Settings | Brightness, estimated battery, available heap, Wi-Fi setup and screen off | Local |
| Radio | Four MP3 station presets, Play/Stop, speaker test, volume and sleep timer | Wi-Fi for stations; test tones work offline |

Press Home or BOOT to return to the calm clock dashboard; Apps opens the launcher. Home activates on touch-down; the touch input is polled every 10 ms. Other buttons activate on release. Hold BOOT for Wi-Fi setup. Settings > Wi-Fi setup scans nearby 2.4 GHz networks on the device. Scroll the list and select an SSID. Secured networks open a masked password field and touchscreen keyboard; use the 1# key for numbers/symbols and press Connect or the keyboard confirmation key. The keyboard uses explicit top alignment so all four rows fit within the display. Saved credentials reconnect through the existing Wi-Fi manager after restart. Up to ten distinct SSIDs/passwords are retained: adding office Wi-Fi preserves home Wi-Fi. Startup scans match all saved names and try available access points in signal-strength order, falling back to other matches if connection fails. Rejoining the same SSID updates that entry; adding an eleventh distinct network removes the last entry. Saved-channel scanning falls back to all channels when it finds no match, so a changed router channel does not require a new password. Hotspot and BluFi provisioning are disabled for this board. WEP/enterprise and hidden SSIDs are not supported by this first direct-join screen. Voice does not start automatically when its screen opens. Home changes the visible screen; the stock voice and wake-word services keep running. Talk / stop uses XiaoZhi’s existing state-dependent control.

The board plays a short, original rising bell chime (C5–E5–G5–C6, 750 ms) while a yellow duck hops left to right for two seconds, then opens the calm clock dashboard. The duck uses native LVGL shapes with a bouncing shadow; its overlay and animation timer are deleted afterwards. Touch input waits until the animation finishes, and first-boot calibration remains available underneath. The chime is generated locally, uses the saved speaker volume including mute, and needs neither Wi-Fi nor an SD card. It runs once per boot; reconnecting Wi-Fi does not repeat it. It connects to saved Wi-Fi without starting XiaoZhi activation or reading an activation number aloud. Voice > Talk / stop explicitly starts the cloud voice setup if needed. This also leaves the audio service available for Radio without requiring a XiaoZhi account. Other board profiles keep their usual automatic voice startup. Until cloud voice is initialized, wake-word detection is disabled in idle; local apps remain available offline.

Home dims to 15% (or the lower selected brightness) after 60 seconds and keeps the clock visible; touching it restores brightness. Other screens turn their backlight off after 60 seconds; the first touch wakes those screens without activating a button. This is backlight saving, not deep sleep: radios and audio services may keep running. Scans and HTTPS requests use a background worker; one request runs at a time. Leaving a screen discards its eventual UI update. Scanning does not disconnect other devices. BLE sends standard scan requests to obtain extra advertisement data, without pairing. Later names replace unnamed entries; complete names take priority over shortened ones. Devices that do not advertise a name remain Unnamed. The first live scan after this update reported 24 devices, 7 named; the name-merging regression checks and board build passed.

The Wi-Fi connection manager consumes `WIFI_EVENT_SCAN_DONE` results, including the event observed from a blocking survey under this SDK. The original survey detected 16 APs but then read zero records. The board now pauses its station handler under the same lock as Wi-Fi setup, performs and reads the passive scan, and resumes the saved connection even if scanning fails. It does not alter the dependency's generated sources. A hardware regression check returned 10 networks, reconnected, and fetched Weather successfully. Scanning works without saved credentials too; a survey refuses to interrupt a connection attempt already in progress.

To repeat the connected-device check after building/flashing, run `python scripts/check_cyberdeck_wifi.py` in the IDF environment and tap Wi-Fi > Refresh within 60 seconds. The check verifies a non-empty result and the subsequent reconnect, without printing SSIDs/passwords. The automatic startup probe used during diagnosis was removed from normal firmware.

That check also passed against the final firmware: 16 networks and saved-network reconnect.

## Internet Radio

Open Radio, press Test for two short quiet tones, then choose Groove Salad, Drone Zone, Secret Agent or BBC World Service and press Play. These are direct HTTPS MP3 streams from [SomaFM](https://somafm.com/) and the [BBC stream](https://stream.live.vc.bbcmedia.co.uk/bbc_world_service). The first visit caps the existing speaker volume at 20; the slider changes the shared speaker volume. The last played station and speaker volume are saved internally. The SD card is optional and is not needed for playback.

Home leaves radio playing so other apps can be used. Stop audio on the launcher stops it. Voice, Wi-Fi setup and Wi-Fi survey stop playback before opening. Voice interruption invalidates the queued radio audio. To change stations, press Stop, wait for Stopped, select the new station and Play. Sleep can stop playback after 15, 30 or 60 minutes; the screen can sleep independently. Stopping clears the timer. A failed stream retries twice, then shows an error. This first version supports the four fixed MP3 presets; it does not include arbitrary URLs, HLS, AAC, podcasts or SD music playback.

The stream worker decodes MP3, mixes stereo to mono, and converts to the existing codec's 24 kHz output. It feeds the shared audio service's bounded queue instead of opening another I2S output. Queue entries carry a playback generation so Stop or a voice interruption prevents the old stream from adding more audio. The output service remains responsible for amplifier power and speaker writes.

For a repeatable connected-device check, run `python scripts/check_cyberdeck_radio.py --quiet-boot --station 3` in the IDF environment. It reboots, checks that voice activation is deferred and the startup chime finishes, waits for Wi-Fi, requests the low-volume tone, checks sustained BBC World Service decoded samples, then stops and restarts twice. The historical `--quiet-boot` option means no spoken setup instructions; the chime still plays. Use `--chime-only` to replay just the chime without changing volume. Omit `--quiet-boot` to test without a restart; `--station 0` tests Groove Salad and sample-rate conversion. USB commands are read from the board's secondary USB console, without changing its primary UART console. The checker sets modem lines before opening to avoid resetting the device. Its pass verifies decoding and queue delivery; listening, touch layout, wake/microphone behaviour and Wi-Fi recovery still require hardware checks.

Validation on 4 October 2026: the English ESP32-S3 firmware and an isolated ESP32-C3 `kevin/c3` build of the shared startup/audio changes passed. All 98 repository host checks passed. The device originally rejected the speaker test with `Voice/device is busy` while its cloud activation prompt read a number aloud. Deferring activation until Talk fixed this. After the Radio screen fix, the final firmware was flashed with hash verification, and the connected-device check passed quiet reboot, saved Wi-Fi connection, tone delivery, BBC MP3 decoding and Stop/restart twice (210,816 and 240,192 queued mono samples). The screen check also passed again after reboot. Audible sound and touchscreen use still require user confirmation; microphone/wake interruption and Wi-Fi recovery during playback remain untested.

BBC World Service's HTTPS endpoint returned HTTP 200 with `audio/mpeg`, without a redirect or ICY metadata interval. A short host sample identified 56 kbps, 24 kHz stereo MP3. The firmware mixes this to mono; the on-device check confirmed that format and sustained queued playback.

Groove Salad also passed the on-device check: 44.1 kHz stereo MP3 converted to 24 kHz mono, with 208,144 and 221,310 samples queued across two Stop/restart runs. Initial connection retries recovered without a reset. The final app image is 2,962,000 bytes with 28% of its app partition free.

The Radio screen originally rebooted the device while saving its first-use volume flag: `radio_volume_ready` exceeded NVS's 15-character key limit, and `Settings::SetInt` aborted. The USB screen check reproduced the crash, and the decoded backtrace identified `ShowRadio`'s settings write. The key is now `radio_vol_init`, with a compile-time length check against `NVS_KEY_NAME_MAX_SIZE`. The invalid old key could never be saved, so no migration is needed. `python scripts/check_cyberdeck_radio.py --screen-only` now opens the real Radio screen, verifies its controls and checks that it stays visible; the same check failed before the fix and passed after installation. This checks device UI state rather than rendered pixels. No temporary debug logs were added.

## Optional SD card

Settings shows the SD card status. Internal NVS remains the source of truth for saved networks, brightness and touch calibration. A background task checks for changed settings every 10 seconds and writes `/cyberdeck/backup.json` on the card, with the previous copy in `backup.bak`. The JSON contains readable SSIDs/passwords, brightness, touch orientation and the device identity. Each temporary file is flushed and read back before replacing the backup. This version exports backups; it does not automatically import card contents into internal memory.

The initial setup can format the inserted card if its filesystem cannot mount, using Dominic's explicit authorisation. That permission is consumed on the first attempt; later failures never trigger automatic formatting. Missing cards and failed writes do not stop the menu or internal credential saving. After a card is inserted or replaced, restart the board to mount it. Card writes run independently of touch and network jobs.

The [LCDwiki ES3C28P documentation](https://www.lcdwiki.com/res/ES3C28P/2.8inch_ES3C28P_ES3N28P_ESP-IDF_Demo_Instructions.pdf) specifies SDIO CLK38/CMD40/D0=39/D1=41/D2=48/D3=47. SD uses its own peripheral, separate from the display SPI bus. The password field uses white text and a lighter placeholder against the dark green background.

USB validation on 3 October 2026: the inserted card mounted at 60,000 MB, and the first backup was written and read-back verified. No formatting was needed. The app update was flashed and its hash verified; an 18-second startup capture showed no panic or abort. The existing 98 host checks passed. Card removal and write-failure fallback have not yet been physically exercised.

## Hardware status

The supplied photographs show an ESP32-S3, 240×320 capacitive display, FT6336G touch chip, ES8311 codec and BAT connector. The electrical configuration is reused from upstream's Freenove 2.8-inch board as the closest match; USB startup has passed. The log confirms 16 MB flash identification, 8 MB octal PSRAM at 80 MHz, and ILI9341/LVGL initialization. The user confirmed visible rendering, landscape touch calibration, Home navigation, Game of Life, Wi-Fi joining, Wi-Fi/BLE discovery, Clock, Weather and Crypto. Battery measurement and audio still need checking.

Display: ILI9341 SPI (CS10/DC46/SCK12/MOSI11/MISO13/backlight45), landscape 320×240. Shared I2C: SDA16/SCL15; touch address 0x38. Audio: MCLK4/BCLK5/WS7/input6/output8/PA1. Battery estimate: ADC1 channel8 and the upstream assumed 200k/200k divider. Do not treat the estimate or charging inference as calibrated.

Before connecting a battery, verify the BAT connector polarity, cell requirements and charging circuit from the seller documentation or measurements. The connector label alone does not establish those details. This firmware does not manage or protect the battery.

First boot displays three red calibration targets: top left, top right and bottom left. Tap each centre and lift your finger between taps. The board checks the samples, detects swapped/mirrored axes, and saves the orientation in NVS. Invalid samples restart the sequence. Settings > Calibrate touch repeats it. The firmware also supports native landscape touch coordinates without rejecting the right side of the screen.

London coordinates and timezone are explicit presets in that same file, not a detected location. ESP32-S3 provides 2.4GHz Wi-Fi and BLE, with no native 5GHz Wi-Fi, Bluetooth Classic, sub-GHz or NFC.

## Build on this Mac

The local compiler is installed in the ignored `.tools/` directory. From the repository root:

```sh
export IDF_TOOLS_PATH="$PWD/.tools/idf-tools"
export PATH="/Users/dominic/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin:$PATH"
source .tools/esp-idf/export.sh
python scripts/build.py dominic-es3c28p-cyberdeck --name dominic-es3c28p-cyberdeck --language en-US
```

On another computer install ESP-IDF 6.1 and source its environment first. Build uses the upstream flash, partition and PSRAM defaults. Verify them against the actual board before flashing. Normal XiaoZhi OTA infrastructure may not have firmware for this private board identity; keep the locally built image for recovery.

## First hardware session

1. Connect USB and identify flash size and PSRAM; back up the entire existing flash before writing firmware.
2. Verify the selected configuration against those readings, then flash the built images.
3. Check screen orientation, all four touch corners and BOOT Home before connecting Wi-Fi.
4. Use Settings > Wi-Fi setup to select a network and enter the password on the board, then test Clock, Weather and Crypto; test Wi-Fi/BLE scans separately.
5. Test audio, brightness/wake and measured battery voltage before portable use.

On 3 October 2026 the complete original 16 MB flash was backed up to `/Users/dominic/ESP32_Backups/original-84c7bb78335c-2026-10-03.bin`, with a companion SHA-256 file. The English cyberdeck firmware was installed and esptool verified each written image. A 12-second startup capture confirmed PSRAM/display initialization without a panic or abort. The user confirmed the visible menu on the first build but reported missed taps. The follow-up build adds on-screen calibration and waits 300 ms after releasing touch reset. A 100 ms wait produced chip ID 00 and rejected polling reads; 300 ms produced ID 64 and successful reads. The user confirmed calibration and Matrix → Home navigation. Calibration saved swap X/Y, no mirror X, mirror Y. Direct Wi-Fi scanning and SSID selection were confirmed by the user photo. The keyboard clipping issue was corrected with explicit top alignment. The user subsequently confirmed Wi-Fi joining, Clock, Weather, Crypto, BLE names, the Wi-Fi survey and Game of Life. Audio and battery calibration remain to be checked. Do not erase the backup.

## Portable power switch candidate

No dedicated power switch is apparent in the supplied board photographs. A practical enclosure candidate is the APEM GH36S010001: a two-position, single-pole panel-mount slide switch with solder lugs. Its silver contacts are rated 3 A at 30 VDC in the [manufacturer's G-series datasheet](https://www.apem.com/storage/datasheets/files/PCB_slideswitches_serie_G/media/PCB-slideswitches-serie-G.pdf). It has mounting holes 17.53 mm apart and a 5.1 mm actuator. [DigiKey lists the exact part](https://www.digikey.com/en/products/detail/apem-inc/GH36S010001/1796125). This is a shopping candidate; enclosure fit and installation have not been verified.

Although sold as ON-ON, it can serve as ON-OFF by wiring the battery positive through the common terminal and one throw, leaving the other throw unconnected and insulated. Battery negative connects directly to BAT negative. Use a switched adapter lead and confirm terminal identity with a continuity check before connecting the battery.

The [LCDwiki schematic](https://www.lcdwiki.com/res/ES3C28P/2.8inch_ESP32-S3_Display_Schematic.pdf) connects BAT+ to both the charger and battery supply path. An inline mechanical switch therefore disconnects the battery from both when off: USB can still power the board, and charging requires the switch to be on. This is a schematic-based expectation, not a charging test on the physical board. No battery has been installed yet.

## Checks and sources

Host logic checks:

```sh
c++ -std=c++17 main/boards/dominic-es3c28p-cyberdeck/tests/logic_test.cc -o /tmp/cyberdeck-logic-test
/tmp/cyberdeck-logic-test
python -m unittest discover -s scripts/tests -v
```

Base: [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32), imported commit `0d576d3d4c049c6f55eaf879725dc23e516511b4`. Hardware reference: [Freenove board implementation](https://github.com/78/xiaozhi-esp32/tree/main/main/boards/freenove-esp32s3-display-2.8-lcd).

Idea reference: [macobt-a11y five-app CYD cyberdeck](https://github.com/macobt-a11y/5in1-cyberdeck-for-ESP32-and-CYD-2.8inch). Its downloadable sketch supplies the clock/weather/crypto/Matrix/Life concept; the new menu, scans and simulations are implemented here instead of importing its TFT_eSPI bundle. [Open-Meteo API](https://open-meteo.com/en/docs) supplies weather; [CoinGecko API](https://docs.coingecko.com/reference/simple-price) supplies prices. XiaoZhi's existing MIT licence remains in the root.

Validation on 4 October 2026: the chime firmware built and flashed with hash verification. The connected-device check passed an actual reboot, startup chime completion, saved Wi-Fi connection without automatic voice activation, speaker test, and BBC World Service Stop/restart twice (229,824 and 214,272 queued mono samples). The user heard the startup chime and confirmed “Sounds good”. Offline-first-boot behaviour and microphone/wake interruption remain untested on hardware.

Run `python scripts/check_cyberdeck_radio.py --startup-only` to reboot and verify duck completion between 2.0 and 2.3 seconds, chime completion, return to Home and absence of automatic activation. It does not need Wi-Fi. Confirm the drawn duck and hopping motion visually on the device.

Startup duck validation on 4 October 2026: the ESP-IDF 6.1 board build passed and the installed image was hash-verified. The user confirmed “Duck appears, then menu”, verifying the visible animation and return to the launcher. The configured animation duration is 2000 ms. The USB startup checker could not obtain restart logs on two attempts; its timing check remains unverified. USB diagnostics received zero lines even after a hardware reset, so the cause has not been established. First-boot calibration after the overlay remains untested on hardware.

## Clock faces

Clock has five faces: Neon (cyan seven-segment digits), Analogue (cream dial), Flip (split digital cards), Orbit (hour/minute/second rings), and Words (the exact time in words). Use the left and right buttons to browse. Press Set default; the button confirms Default saved. Home then Apps > Clock opens the saved favourite, including after a restart. Browsing alone does not overwrite it. This selects the default Clock face; startup opens the separate calm analogue dashboard after the duck.

All faces use the existing Europe/London timezone, including UK summer time, and update once per second. The initial favourite is Neon. An unsynchronised clock says TIME NOT SET and offers Wi-Fi or Apps > Settings > Set date / time rather than showing a false time. Wi-Fi sets the time through the existing NTP worker; once synced, the clock continues offline until it loses its time. The clock-face preference is the ten-character `clock_face` NVS key in `cyberdeck`; internal storage is primary and the existing optional SD namespace backup includes it.

The faces share the existing PSRAM RGB565 canvas buffer with Matrix/Life. No new font assets, network APIs or animation libraries are required. To repeat the renderer check, compile `tests/clock_test.cc` and `cyberdeck_clock.cc` in this directory with C++17 and address/undefined-behaviour sanitizers, then run the result. An optional output-directory argument writes five PPM previews from the production renderer. The check covers distinct faces, changing seconds, all minutes around midnight/noon/end-of-day, buffer guards and invalid/unsynchronised time. On the device, browse all five styles, save a favourite, return Home and reopen Clock, then restart and reopen Clock to check persistence.

Five-face clock validation on 4 October 2026: address/undefined-behaviour sanitized host renderer check passed all five distinct faces, changing seconds, every minute at midnight/noon/end-of-day, guarded buffers and invalid/unsynchronised time. The existing board logic check also passed. Rendered production previews were visually inspected. The ESP-IDF 6.1 S3 build passed; the installed image was hash-verified, 2,962,000 bytes with 28% app partition free. USB logging is available again (187 lines received in a 20-second capture); the earlier zero-line issue has no established cause. Face browsing and favourite persistence on the touchscreen still await user confirmation. No shared/core changes were made in this clock update.

## Calm home and offline use

The selected third visual concept is the default: an ivory analogue dial, actual London weather, date, Wi-Fi connection state, battery estimate and two large Radio / Apps buttons. Tapping a set dial opens the existing five-face Clock app; its saved favourite remains independent of this analogue dashboard. Home and BOOT always return here. Radio does not autoplay; its existing Play action explains when internet is required. A connected Wi-Fi icon means association with an access point, not proof of internet access.

Time synchronises automatically in the background and resynchronises after reconnection. It continues locally while powered without Wi-Fi. After complete power loss it needs a new sync or a manual setting. An unset dial has a Set time button; the same picker is available under Apps > Settings > Set date / time. Five rollers select year/month/day/hour/minute in London local time. Invalid calendar dates and the missing spring daylight-saving hour are rejected. Saving sets seconds to zero; a later Wi-Fi sync can correct it. No saved timestamp is presented as current after power loss.

Weather refreshes every 15 minutes while connected, retrying failures after one minute. A validated Open-Meteo reading includes its real condition code and UTC observation time. A single `cyberdeck/wx_cache` JSON value stores temperature, condition and observation timestamp internally; the optional SD backup includes it. Missing/failed SD does not prevent use. An internal cache-write failure retains the RAM reading without rebooting. On restart, offline, after a fetch failure, or when the observation is over 30 minutes old, the card shows Cached plus its dated timestamp. Before the first reading it shows Weather unavailable. Cached weather never sets the device clock. Codes and Unix timestamps follow the [Open-Meteo documentation](https://open-meteo.com/en/docs).

Battery defaults to an unavailable marker. Only enable Apps > Settings > Battery fitted after installing a cell. The board now calls the existing native ADC estimator directly, rejects failed/non-finite/out-of-range readings and displays approximate percentages (`~76%`). It does not claim charging or USB-power detection. GPIO9 / ADC1 channel8 and the assumed 200k/200k divider still require physical validation with the chosen cell; this is a voltage estimate, not a calibrated fuel gauge.

Checks: compile/run the existing `tests/logic_test.cc` with C++17 and address/undefined-behaviour sanitizers for valid dates, London summer/winter offsets, DST boundaries and offline/stale weather. The clock renderer check also covers Home seconds, condition icons, unset time and guarded buffers. `python scripts/check_cyberdeck_radio.py --home-only --port <port>` checks actual Home controls, widget layout and retention through USB; `--startup-only` checks the duck/chime and dashboard return. Touch, manual-picker usability, battery calibration and full-power-loss behaviour need physical confirmation.

Calm-home validation on 5 October 2026: both board host checks passed with address/undefined-behaviour sanitizers, including calendar/DST validation, weather freshness/offline states and Home renderer buffer boundaries. ESP-IDF 6.1 build passed; the 3,214,128-byte application has 22% app partition free and flashing was hash-verified on the original board MAC. The USB Home check passed actual widget fit and retention with clock unset / weather unavailable / Wi-Fi offline. Startup check passed duck completion at 2015 ms, chime completion and Home return with cloud activation deferred. A separate 35-second capture received 266 lines with no observed crash, no network-connected event and no cache-write failure. Thus live-weather caching/reboot recovery and reconnection were not physically verified this time; manual-picker touch usability and battery calibration also await physical confirmation.

The user physically confirmed the calm Home looks good and both Radio and Apps buttons work on 5 October 2026.
