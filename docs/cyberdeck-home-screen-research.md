# Home-screen ideas for the portable cyberdeck

Prepared: 5 October 2026. Status: research and design proposals; no firmware changes in this research task.

## Recommended direction

Keep the two-second duck and chime, then land on a useful clock dashboard. Give the time the most space, keep Wi-Fi and battery status visible, and use large, labelled shortcuts to reach Radio and Apps. Keep the existing app menu one tap away. This recommendation adapts small-device patterns below; it does not claim their firmware is compatible with this board.

## Five practical directions

| Direction | What the original project establishes | Adaptation for this 320×240 landscape screen |
|---|---|---|
| **1. Clock dashboard — recommended** | InfiniTime starts on a digital watch face, places battery and connection indicators at the top, and opens applications separately. [First-party getting-started guide](https://github.com/InfiniTimeOrg/InfiniTime/blob/main/doc/gettingStarted/gettingStarted-1.0.md) | Large time and smaller day/date; a slim status strip; two or three labelled bottom shortcuts. Use the favourite existing clock style. A visible Apps button avoids requiring the user to discover a swipe. |
| **2. Clock + weather sidebar** | PineTimeStyle puts time on the left and information in a right sidebar, with optional current weather and an explicit missing-data display. [PINE64 layout and weather documentation](https://pine64.org/documentation/PineTime/Watchfaces/PineTimeStyle/) | Time occupies roughly two thirds of the width; temperature and a condition icon occupy the remainder. Show unavailable or stale weather clearly. Battery/Wi-Fi belong in a consistent top strip rather than competing with the weather tile. |
| **3. Pocket radio home** | MiniWebRadio combines touchscreen radio, digital clock, volume, brightness and sleep timer; its documented displays include ILI9341 320×240. [Project README](https://github.com/schreibfaul1/ESP32-MiniWebRadio) | Time/date remain above a BBC World Service or favourite-station card. One large Play/Stop action, with Apps beside it. This is a good optional default for frequent listening. Do not automatically play at boot. Supported resolution does not establish compatibility with our integrated ES8311 board. |
| **4. Terminal-style cyberdeck** | InfiniTime includes a Terminal watch face, and its source handles time, battery, connection and weather information. [Watch-face list](https://github.com/InfiniTimeOrg/InfiniTime/blob/main/doc/gettingStarted/Watchfaces.md), [Terminal implementation](https://github.com/InfiniTimeOrg/InfiniTime/blob/main/src/displayapp/screens/WatchFaceTerminal.cpp) | Dark background, cyan/green accent, large monospaced time, and simple labels such as Wi-Fi Connected and Battery ~76%. Keep the decorative terminal look while retaining large normal touch buttons; avoid filling the home screen with logs or scan results. |
| **5. Calm cards / media dock** | LVGL's native demos demonstrate themed widgets and a music-player interface. The music demo is designed primarily for 480×272 or 272×480, so it needs simplification for this smaller screen. [LVGL demo documentation](https://github.com/lvgl/lvgl/blob/master/demos/README.md) | One dominant clock card, one secondary weather or Now Playing card, and a fixed Apps button. Use the widgets already in the firmware; importing the entire demo is unnecessary. Avoid a dense grid of miniature widgets. |

## Proposed clock-dashboard layout

All dimensions below are starting points for a prototype, not verified touch targets on this hardware.

- **Top, about 26 pixels:** Wi-Fi icon plus Connected / Connecting / Offline text on the left; battery icon plus estimated percentage or an unavailable marker on the right. Status should remain visible on ordinary app pages wherever their layout permits.
- **Middle, about 150 pixels:** large time, smaller weekday/date, and one optional information line. Tapping the clock opens its existing style chooser. If the time has not synchronised, display a clear waiting state rather than an apparently valid 1970 date.
- **Bottom, about 48 pixels:** two wide buttons, Apps and Radio. A third Weather button is possible if physical testing confirms comfortable targets. A playing radio should expose Stop or a clear route to the existing Stop control.
- **Navigation:** Home means this dashboard; Apps means the existing launcher. The saved clock-face preference controls the initial appearance. A boot-page preference could later select Clock, Clock + Weather or Radio without conflating that with the clock style.

The proposed text and buttons supplement colour and icons, so connection state and important actions remain understandable without relying on colour alone. Preserve contrast in the password field and avoid the earlier clipped keyboard layout.

## What battery percentage can honestly mean here

The board currently constructs `AdcBatteryMonitor` on ADC1 channel 8, assumes a 200k/200k divider, and supplies `GPIO_NUM_NC` for charging detection. Its percentage is a voltage-derived estimate, and charging status comes from software estimation rather than a wired charger-state signal. The board documentation says battery sensing is uncalibrated and no battery has been installed. [Board implementation](../main/boards/dominic-es3c28p-cyberdeck/cyberdeck_board.cc), [battery monitor implementation](../main/boards/common/adc_battery_monitor.cc), [board hardware notes](../main/boards/dominic-es3c28p-cyberdeck/README.md)

There is a further validity issue: the shared monitor currently returns 100% when its handle is missing or a capacity read fails, and the board getter always reports success. That value must not be treated as proof of a fitted, full battery. Reliable battery absence, USB-power status and charging indication are not established by these APIs. [Capacity/error handling](../main/boards/common/adc_battery_monitor.cc), [board getter](../main/boards/dominic-es3c28p-cyberdeck/cyberdeck_board.cc)

Proposed presentation: use **Battery —** or **Battery not configured** until a cell is fitted and measurement checked; then show **~76%** with an estimated label available in Settings. Do not claim **Charging** or **USB powered** without an independently verified signal. Keep the divider/cell calibration adjustable rather than encoding false precision in the home screen.

## Scope of a first version

The clock dashboard offers the largest immediate usability gain because time, clock styles, Wi-Fi connection state and Radio already exist. Start with that screen, an honest battery state, and two large shortcuts. Weather sidebar, selectable boot pages and Now Playing are separate design options. No alarm, new sensors or new audio formats are required for the first version.

Research checks: opened the linked first-party pages and traced the local battery getter and error handling. Hindsight knowledge pages were queried but still contained only “Generating content...” and contributed no technical facts. Hardware usability and calibrated battery measurement remain untested by this research task.

## Visual concepts shown to the user

The three independent generated images appeared in this order in the chat on 5 October 2026; this is the authoritative selection mapping:

1. Clock-first home — large cyan digital clock, status bar, Radio and Apps. Image: `/Users/dominic/.codex/generated_images/01a10351-0d8f-7bd3-ad9c-0388affce1ed/exec-e71ae5c6-19c1-4514-9cb0-a3b1f11349df.png`.
2. Pocket radio home — compact clock, World News paused station placeholder, large Play, Clock and Apps. Image: `/Users/dominic/.codex/generated_images/01a10351-0d8f-7bd3-ad9c-0388affce1ed/exec-2e887730-f8b5-4c74-99b8-731a70370679.png`. Actual implementation can use the existing BBC World Service preset; station text in this concept is illustrative.
3. Calm glance home — ivory analogue clock, weather and update time, Radio and Apps. Image: `/Users/dominic/.codex/generated_images/01a10351-0d8f-7bd3-ad9c-0388affce1ed/exec-63b5acff-5333-401c-bb4d-c2dd82587b91.png`.

All are 4:3 landscape concepts for the logical 320×240 display; their battery, time and weather readings are illustrative. Some generated battery labels omit the approximation sign, but the implementation must retain the honest estimated/unavailable presentation described above. Preserve readable type and thumb targets when adapting the concepts to the actual TFT. Keep Wi-Fi connection distinct from verified Internet access. A dimmed clock / keep-visible display option was proposed, with a separate pocket timeout option; automatic USB-power detection is not established.

The recommended direction is the clock-first home. The user selected concept 3 (Calm glance home) and authorised its implementation with manual offline time setting and cached weather. No home-screen firmware was changed, built or flashed during this ideas task.

The implemented dashboard uses the third concept. Its analogue layout is independent of the five-face Clock app favourite; the existing app launcher is reached through Apps. See the board guide for current behaviour and validation.
