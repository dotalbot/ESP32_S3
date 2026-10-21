# Freenove 3.5-inch cyberdeck

Board profile: **`dominic-fnk0104n-cyberdeck`**, ESP32-S3, 16 MB flash, 8 MB octal PSRAM. This is the FNK0104N capacitive-touch ST77922 model, with a 480 × 320 landscape interface, upright with **USB on the right and the microphone on the left**. Do not flash this profile onto the original 2.8-inch board or the ST7796 Freenove models.

## Interface

The duck and chime lead into the ivory clock/weather Home screen. The larger screen has a larger analogue dial, readable weather/date text, visible Wi-Fi and estimated battery status, and larger Radio and Apps buttons. Other app controls use the extra width and height; Matrix, Life and the five clock faces scale to fit. Home and BOOT return to the dashboard. Settings retains touch calibration and brightness control.

The RGB status LED on GPIO40 uses the shared firmware indicators: slow blue flashing means Wi-Fi setup mode, rapid blue flashing means startup, and steady blue means connecting. It turns off in idle. To join a network, use Settings → Wi-Fi setup on the screen. Green is used during radio/chime playback; these indications do not measure battery charging.

All existing apps are reused: Wi-Fi survey and on-device password entry, BLE discovery, five clock faces with a saved favourite, weather, crypto, Matrix, Life, Radio (including BBC World Service), and opt-in XiaoZhi Voice. Cloud activation does not run automatically. Up to ten Wi-Fi credentials are saved internally on this board. They are not copied from a different physical board. Without Wi-Fi, local apps still work; set the clock manually after complete power loss, and weather displays cached/unavailable status.

The fitted battery is enabled by default. Home shows an approximate percentage, and Settings also shows voltage. Voltage uses the chip's ADC calibration, twelve samples and the documented 1:1 resistor divider. Settings offers a 90–110% voltage correction, saved internally; leave it at 100% unless comparing with a multimeter. This adjusts the reading, not the charger. Percentage is a voltage-based estimate and varies with load and charging; there is no wired charging-status input, so the app does not claim to detect USB power or charging. Missing/invalid ADC readings show unavailable.

SD storage is optional, with internal settings remaining primary. This new profile does **not** automatically format an unmountable card. Existing card contents must be preserved. See the [shared app guide](../dominic-es3c28p-cyberdeck/README.md) for app behaviour and backup details.

## Verified wiring

| Function | FNK0104N wiring |
|---|---|
| ST77922 QSPI | CS10, CLK12, D0=11, D1=13, D2=14, D3=9; 40 MHz in this firmware |
| Display/backlight | Native 320 × 480; software rotation 90° plus panel mirror X/Y for USB-right mounting; GPIO41 backlight |
| Touch | Manufacturer protocol at I2C `0x55`, 16-bit registers; SDA38, SCL39, reset48; polling |
| Battery ADC | GPIO8 / ADC1 channel7, two 100k resistors; cell voltage = 2 × ADC voltage |
| ES8311 / amplifier | I2C SDA38/SCL39, PA enable1 |
| I2S | MCLK17, BCLK18, input16, output15, WS21 |
| SDMMC | CLK5, CMD4, D0=6, D1=7, D2=2, D3=3 |

The source and schematic references are in the [hardware inspection](../../../docs/cyd-3.5-inspection-2026-10-21.md). GPIO9 is display data on this board: the original board's battery GPIO must not be reused.

The installed ST77922 driver does not swap the display axes in hardware by default. The display port therefore rotates pixels in software. LVGL also rotates pointer input: touch calibration maps readings to logical screen positions, then the callback supplies their unrotated panel coordinates to LVGL. Applying the screen transform twice caused the first build's incorrect taps. Default logical mapping is swap XY, mirror X, no mirror Y. The panel is mirrored on both axes to turn the original upside-down landscape image 180°; touch payloads receive the same normalization before calibration. Existing calibration values remain valid, and manual calibration remains supported.

ST77922 also requires both physical X window boundaries to be divisible by four; see the installed driver's `README.md`, Notes, and the existing LCDwiki ES3C35P rounder. Without this, small status updates produced diagonal/garbled characters even though full-page drawing looked correct. `Fnk0104nRoundArea` rounds logical Y outward to four-pixel boundaries because software rotation maps Y to physical X. It is registered once on this board's LVGL invalidation event before creating the app UI. LVGL then draws the expanded region and splits it into aligned strips; enlarging the window after rendering would send the wrong pixel data. Other board profiles are unaffected.

## Build and install

Use the ESP-IDF 6.1 environment from the repository root:

```sh
python scripts/build.py dominic-fnk0104n-cyberdeck --name dominic-fnk0104n-cyberdeck --language en-US
idf.py -p /dev/cu.usbmodem1101 -b 460800 flash
```

Rebuild subsequent changes with `idf.py build`; `idf.py ... app-flash` updates only the app after the initial full install. Check the selected board and actual connected USB identity before flashing. The canonical build command changes the active build configuration.

The custom `partitions/v2/fnk0104n-16m.csv` retains the factory NVS range `0x9000–0xdfff` and OTA metadata offset `0xe000`. App/asset locations differ from factory firmware, so the initial install requires the complete build. No erase-all is needed. The original 16 MB factory image was privately backed up and SHA256-verified before installation at `/Users/dominic/dev_projects/ESP32_Backups/original-b81f3fc529a0-2026-10-21.bin`; do not commit firmware backups, which can contain credentials.

## Validation — 21 October 2026

- ESP-IDF 6.1 builds passed for this profile, the original 2.8-inch profile, and a representative ESP32-C3 board. All 98 repository host tests passed.
- Sanitized board logic and clock-renderer tests passed, including battery bounds/monotonicity, touch payload validation, all eight calibration orientations, and guarded 480 × 210 Home rendering.
- The native LVGL regression reproduced incorrect touch input, then passed all 153,600 screen pixels after the fix. The follow-up checks the USB-right panel orientation and decodes touch payloads with the actual board mirror settings. It exercises LVGL's real pointer-rotation function, not a mock of it.
- The larger-board firmware was installed with flash hash verification. The USB Home check passed actual widget fit/retention at 480 × 320, offline with time unset. Calibrated battery readings were approximately 3974 mV / 78% while attached to USB.
- The user confirmed upright display and working taps after the 180° correction (3,362,240-byte image, hash-verified app flash). The earlier USB Radio controls/retention check passed after waiting for actual startup completion (duck: 2040 ms).
- `IMG_5856.jpeg` then showed garbled Wi-Fi/battery text at the top; Apps also briefly showed a corrupted line. The native status-update regression reproduced unaligned panel windows using actual LVGL, the production 16-pixel font and the same small draw buffer. After registering the alignment callback, status updates at all 320 vertical positions stayed in bounds and met four-column alignment; the touch regression still passed. The 3,362,304-byte image built and app-flashed with hash verification; the original-board build also passed. USB Home checks passed fit/retention after a 2020 ms duck, with battery reading 4006 mV / approximately 82%. The user then confirmed “Both look clear now”: the Wi-Fi/battery strip is readable and Apps opens without the fuzzy line.

Repeat the touch regression in the IDF environment:

```sh
cmake -G Ninja -S main/boards/dominic-es3c28p-cyberdeck/tests -B /tmp/cyberdeck-touch-test
cmake --build /tmp/cyberdeck-touch-test -j8
ctest --test-dir /tmp/cyberdeck-touch-test --output-on-failure
```

USB Home/Radio checks use `scripts/check_cyberdeck_radio.py --port /dev/cu.usbmodem1101 --home-only` or `--screen-only`. The checker waits for startup before opening diagnostic screens, because native USB attachment may restart the board. The radio screen check verifies controls/retention, not sound or network playback.

Still needs physical confirmation on this board: keyboard, sound, live network apps, SD mounting/removal, battery-only operation/runtime and multimeter voltage comparison. Charging status cannot be determined from this board's available GPIO wiring.
