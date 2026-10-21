# Freenove 3.5-inch cyberdeck

Board profile: **`dominic-fnk0104n-cyberdeck`**, ESP32-S3, 16 MB flash, 8 MB octal PSRAM. This is the FNK0104N capacitive-touch ST77922 model, with a 480 × 320 landscape interface, upright with **USB on the right and the microphone on the left**. Do not flash this profile onto the original 2.8-inch board or the ST7796 Freenove models.

## Interface

The duck and chime lead into the saved light/dark clock/weather Home screen (ivory by default). The larger screen has a larger analogue dial, readable weather/date text, visible Wi-Fi and estimated battery status, and five compact icon buttons: **headphones = Radio, nine-square grid = Apps, moon = dark/light mode, sun = brightness slider, power symbol = display off**. Display off only turns off the backlight; radio playback continues. A touch wakes the display without activating a control; release and tap again to use it. BOOT also returns to Home and restores brightness. Other app controls use the extra width and height; Matrix, Life and the five clock faces scale to fit. Settings retains touch calibration and brightness control. Apps now includes **Dark mode** (or **Light mode** when active), beside Radio and Stop audio. It switches the Home dashboard/dial between ivory and dark green, with readable weather/Wi-Fi/battery text, then returns Home. The preference is saved internally as `cyberdeck/dark_mode`, so it survives restart without an SD card. Other app pages already use dark backgrounds; clock-face artwork retains its individual style. Home's sun button opens a brightness popup: drag to adjust live from 10–100%, release to save internally, and tap Done to close. Brightness is independent of dark/light mode; Settings uses the same slider behaviour. The popup is closed safely on navigation. Saving happens separately from the backlight fade, including when the selected level already matches the current output.

Weather is preset to **Colchester (51.8892, 0.9042)**, with London/UK timezone rules retained. Colchester is named beside the Home date. Its internal cache uses `cyberdeck/wx_colchester`, so the previous London `wx_cache` value cannot appear as Colchester weather. Without a Colchester reading it shows unavailable; after a successful fetch the existing dated offline cache applies. The original 2.8-inch profile retains its London preset and cache. The Weather app now shows current temperature/conditions, today's forecast high/low, wind, humidity and a gentle 96 × 96 animated symbol. Clear skies use a sun or moon; rain, snow, fog and storms have distinct symbols. Home stays calm. Current readings and the daily range are cached internally; old caches without a range remain valid. After midnight or with time unset, a saved range is labelled with its date rather than “Today”. The duck reaction game remains in the backlog.

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

Still needs physical confirmation on this board: password keyboard, remaining network apps, SD mounting/removal, battery-only operation/runtime and multimeter voltage comparison. Weather/radio checks added below cover those specific apps, not every network function. Charging status cannot be determined from this board's available GPIO wiring.

### Colchester and Home icon update — 21 October 2026

The 3,363,120-byte application built and app-flashed onto the identified FNK0104N with hash verification, preserving NVS. The original 2.8-inch profile also built in its isolated regression directory. All 98 host tests and both native LVGL touch/partial-redraw regressions passed in the IDF environment. Formatting checks passed for changed C++ lines.

The updated USB Home check verifies all three buttons fit, date text clears the controls, and the headphones/power glyphs exist in the linked font. It passed twice at 480 × 320 after a 2020 ms duck/chime, with Wi-Fi connected and the clock set. No old London weather appeared: Colchester initially showed unavailable. A host request using the new coordinates returned the expected current-weather fields. Dominic then confirmed **all three icons work, including display-off and touch-to-wake without opening an app**. These checks do not establish live on-device weather or battery calibration.

### Richer Weather and live field checks — 21 October 2026

The 3,367,792-byte application built and app-flashed with hash verification, preserving NVS. The original 2.8-inch profile also builds. All 98 host tests passed; the native touch, partial-redraw and new Weather tests pass. The Weather check covers invalid/null/ranged forecast data, dated offline forecasts across both UK DST changes, distinct rain/snow/day/night symbols, animation and guarded buffer bounds. Address/undefined-behaviour sanitizer checks passed.

On the identified FNK0104N, USB checks observed a saved daily range after restart, then live Colchester current weather (8.7 C during the check), a valid forecast range and non-overlapping Weather labels. Startup previously attempted DNS before DHCP completed; automatic weather/time work now waits for the connection manager's GOT_IP state. No DNS failure appeared on the final startup check.

BBC World Service decoded 24 kHz stereo MP3 into 24 kHz mono. Queued PCM samples increased from 230,976 to 471,744 with the display off. Dominic confirmed both the quiet test tones and BBC sounded good. A requested station disconnect returned ESP_OK, emitted a disconnect event, received a new IP lease and returned to connected/fresh-weather Home without password entry. This checks reconnection to the currently available saved network; moving between two different saved SSIDs remains a separate acceptance test. Battery runtime and long-duration radio/outage behaviour remain unmeasured.

The bounded USB diagnostic console now also accepts `weather screen`, `weather status`, `display off` and `wifi reconnect`. Weather status checks real label bounds/order; display off uses the same method as the Home and Settings buttons. These commands change only this attached device. Radio decoding/queue checks establish audio delivery; Dominic's listening check establishes audible playback.

Daily max/min, current `is_day`, Unix timestamps and the explicit Europe/London forecast timezone follow the [Open-Meteo API documentation](https://open-meteo.com/en/docs). A missing/invalid daily range does not discard otherwise valid current weather. Existing current-only cache records remain readable, with high/low shown as unavailable until refreshed.

### Weather visibility correction — 21 October 2026

Dominic reported a still moon with the readings invisible on the dark Weather page. The original layout check only asserted positions, so it did not catch poor contrast. A native LVGL reproduction measured inherited text luminance 33 against background 16: the default body theme overrides the root's inherited colour. Weather labels now explicitly use the pale foreground (luminance 230), and the real-device Weather diagnostic also rejects dark/transparent text. Home colours remain unchanged.

The moon previously stayed still while a tiny star moved one pixel. It now gently bobs by four pixels, and the renderer regression requires more than 100 pixels to change between representative moon frames. That check failed before the fix and passes afterwards, including under address/undefined-behaviour sanitizers. Both board builds and all three native tests passed. The 3,368,032-byte corrected app was installed with flash hash verification. USB checks passed fresh Colchester data, forecast range, label fit and readable text; Dominic then confirmed “Yes, text and moon work”. This physical acceptance supersedes the earlier widget-fit-only result.

### Saved dark / light Home mode — 21 October 2026

The 3,369,792-byte image built and app-flashed to the identified FNK0104N with hash verification, preserving NVS. Both larger/original board builds passed. All four native tests passed (touch, partial redraw, Weather, Clock); the Clock renderer also passed address/undefined-behaviour sanitizer checks for both Home sizes, visible light clock markings against a dark background, guarded buffers, unset time and exact restoration of the light image.

USB diagnostics exercised the actual Apps-button click event, rather than setting the preference directly: Dark mode and Light mode captions fit; dark Home colours/layout matched; dark mode survived reboot; Light mode restored the light Home; then Dark mode was selected again. Home battery/weather text and background are checked against the shared palette. Dominic subsequently confirmed both modes work and look good, then requested the direct Home shortcut and brightness popup below. New bounded diagnostic commands are `theme menu` and `theme toggle`. The radio worker is not stopped by changing Home mode.

### Five Home controls and brightness popup — 21 October 2026

The expanded 3,372,256-byte application built and app-flashed with hash verification, preserving saved settings. Both board profiles build and all four native tests pass. Home retains its row height and adds a direct moon theme button and a sun brightness button, with spacing adapted to both display widths. The Apps theme entry remains.

On FNK0104N, USB diagnostics exercised both new Home buttons through their actual LVGL click handlers. Both theme directions returned matching Home colours. All five controls and the brightness popup fit. Slider value/release events changed brightness from 60% to 35%; the backlight output reached 35%, the saved value read back as 35%, and both persisted after reboot. Invalid requests (9, 101 and malformed text) were rejected. The original 60% level was restored, saved and observed at the output before returning Home. The brightness persistence uses a separate save on release because the existing backlight method skips a permanent write when output already equals the requested level. The shared Settings slider now uses the same live/release handling.

The popup is deleted when navigating away, including BOOT/Home, and Done closes it without leaving Home. Bounded USB diagnostics `home theme`, `brightness menu` and `brightness <10..100>` support these checks. Physical touch/visual acceptance is recorded separately from synthetic event checks. Battery runtime remains unmeasured.
