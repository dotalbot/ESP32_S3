# Dominic's portable 2.8-inch cyberdeck

A touchscreen launcher built into XiaoZhi, combining the five-app CYD idea with wireless discovery. Select the unique `dominic-es3c28p-cyberdeck` variant; the existing Freenove variant stays separate.

## Menu

| App | Behaviour | Connection |
|---|---|---|
| Wi-Fi | Passive 2.4GHz survey: name, channel, signal and security; up to 24 results | Briefly pauses saved-network connection, then reconnects automatically; wait if already connecting |
| BLE | Active five-second scan: name, address and signal; merges scan responses; up to 24 results | No internet |
| Clock | London time with UK daylight saving; NTP sync when opened online | Internet initially; runs locally afterwards |
| Weather | London temperature, humidity and wind from Open-Meteo | Internet; manual refresh |
| Crypto | Bitcoin, Ethereum, Solana and Dogecoin in GBP with 24-hour changes | Internet; manual refresh; public API limits apply |
| Matrix | Green pixel-glyph falling-rain animation | Offline |
| Life | Conway's Game of Life on a wrapping 32×18 grid; reopen to reseed | Offline |
| Voice | Stock XiaoZhi screen; tap Talk / stop to interact | Wi-Fi, XiaoZhi activation/server and audio hardware |
| Settings | Brightness, estimated battery, available heap, Wi-Fi setup and screen off | Local |

Press Home or press BOOT to return to the launcher. Home activates on touch-down; the touch input is polled every 10 ms. Other buttons activate on release. Hold BOOT for Wi-Fi setup. Settings > Wi-Fi setup scans nearby 2.4 GHz networks on the device. Scroll the list and select an SSID. Secured networks open a masked password field and touchscreen keyboard; use the 1# key for numbers/symbols and press Connect or the keyboard confirmation key. The keyboard uses explicit top alignment so all four rows fit within the display. Saved credentials reconnect through the existing Wi-Fi manager after restart. Hotspot and BluFi provisioning are disabled for this board. WEP/enterprise and hidden SSIDs are not supported by this first direct-join screen. Voice does not start automatically when its screen opens. Home changes the visible screen; the stock voice and wake-word services keep running. Talk / stop uses XiaoZhi’s existing state-dependent control.

The screen backlight turns off after 60 seconds without touch. The first touch wakes it without activating a button. This is backlight saving, not deep sleep: radios and audio services may keep running. Scans and HTTPS requests use a background worker; one request runs at a time. Leaving a screen discards its eventual UI update. Scanning does not disconnect other devices. BLE sends standard scan requests to obtain extra advertisement data, without pairing. Later names replace unnamed entries; complete names take priority over shortened ones. Devices that do not advertise a name remain Unnamed. The first live scan after this update reported 24 devices, 7 named; the name-merging regression checks and board build passed.

The Wi-Fi connection manager consumes `WIFI_EVENT_SCAN_DONE` results, including the event observed from a blocking survey under this SDK. The original survey detected 16 APs but then read zero records. The board now pauses its station handler under the same lock as Wi-Fi setup, performs and reads the passive scan, and resumes the saved connection even if scanning fails. It does not alter the dependency's generated sources. A hardware regression check returned 10 networks, reconnected, and fetched Weather successfully. Scanning works without saved credentials too; a survey refuses to interrupt a connection attempt already in progress.

To repeat the connected-device check after building/flashing, run `python scripts/check_cyberdeck_wifi.py` in the IDF environment and tap Wi-Fi > Refresh within 60 seconds. The check verifies a non-empty result and the subsequent reconnect, without printing SSIDs/passwords. The automatic startup probe used during diagnosis was removed from normal firmware.

That check also passed against the final firmware: 16 networks and saved-network reconnect.

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
