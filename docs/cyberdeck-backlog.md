# Pocket Cyberdeck — feature backlog and delivery status

Last reviewed: **21 October 2026**. Current hardware: **Freenove FNK0104N, 3.5-inch capacitive touch, 480 × 320 landscape**, with a battery attached. The original 2.8-inch profile remains supported separately.

This register combines Dominic's requests in this conversation, the checked-in firmware, board guides and the earlier ideas documents. **Implemented means present in the code; it does not mean every function has been physically verified on the larger board.** Priorities below are suggested next steps, not permission to implement every idea.

> 🧠 **From Hindsight memory (Key decisions and rationale)** — The agreed starting scope combined the five dashboard apps with Wi-Fi/BLE discovery on XiaoZhi-based firmware. The current source confirms that scope. The memory page's older hotspot setup description is superseded by the on-device scanner and keyboard now implemented.

## What is implemented

| ID | Requested feature | Current behaviour | Evidence / remaining verification |
|---|---|---|---|
| UI-01 | Several abilities behind a menu | Apps launcher with Wi-Fi, BLE, Clock, Weather, Crypto, Matrix, Life, Voice, Settings and Radio; Home and physical BOOT return to the dashboard. | Source [S1]; navigation confirmed by Dominic on both boards. |
| UI-02 | A useful default screen instead of the menu | Selected cream analogue clock/weather dashboard, date, Wi-Fi state and approximate battery percentage. Home now has icon buttons for Radio, Apps, dark/light mode, brightness and display off, with touch-to-wake. | [S1, S2]; installed; USB three-button fit/font/retention checks passed. Dominic confirmed all three actions and touch-to-wake work. |
| UI-06 | Dark mode from Home and the main Apps menu | Reversible Dark mode / Light mode item plus a direct Home moon button; themes Home, analogue dial and status/weather text. Saves `cyberdeck/dark_mode` internally across restart. | [S1, S2]; implementation/build checks recorded in the board guide; Dominic confirmed both modes work and look good; the requested Home shortcut is included in the expanded slice below. |
| UI-07 | Brightness control directly from Home | Sun button opens a live brightness slider, 10–100%, saved on release with Done to close. Internal storage works without SD. Settings shares the same handling; changing theme does not change brightness. | [S1, S2]; expanded delivery checks below. |
| UI-03 | Yellow jumping duck for two seconds | Local duck animation before Home, independent of internet/SD. | [S1]; Dominic confirmed duck then menu during development; current destination is Home. |
| UI-04 | Cute startup sound | Short locally generated rising chime; respects saved volume/mute. | [S3]; Dominic said it sounded good on the original device. Larger-board sound needs its own check. |
| UI-05 | Stop spoken startup activation instructions | Cloud activation is deferred until explicitly using Voice; normal boot keeps the duck/chime. | [S1, S3]; recorded USB startup checks passed. |
| CLK-01 | Five clock styles | Neon, Analogue, Flip, Orbit and Words. | [S4]; renderer/build checks recorded. Touchscreen browsing on the larger board still needs confirmation. |
| CLK-02 | Save a default clock | Set default saves the favourite used when opening Clock, including after restart. | [S1, S4]; persistence needs a physical acceptance check. The Home dashboard remains its own analogue layout. |
| CLK-03 | Useful operation without Wi-Fi | Clock continues while powered; manual date/time setting and clear unset-time state after full power loss. London timezone/DST supported. | [S1, S4]; manual-entry and power-loss checks remain. No battery-backed RTC is implemented. |
| NET-01 | Scan and join Wi-Fi on the device | Nearby 2.4 GHz network list, select SSID, masked password field, touchscreen keyboard and Connect. | [S1, S5]; joining confirmed on the original board; larger-board keyboard/joining needs confirmation. |
| NET-02 | Keep office and home credentials | Up to ten saved networks in internal storage; scans try available saved networks, with connection fallback. | [S5]; live switching between two saved networks remains a larger-board acceptance task. Credentials are local to each device. |
| NET-03 | Wi-Fi survey | Up to 24 results with SSID, channel, signal and security; saved connection resumes afterwards. | [S1, S5]; original-board empty-list regression fixed and confirmed by Dominic. Survey stops radio playback. |
| NET-04 | BLE discovery, including names | Five-second active scan, up to 24 devices, name/address/signal; merges advertised names and scan responses. | [S1]; Dominic confirmed some names now appear. Devices that publish no name remain unnamed. |
| APP-01 | Weather | Larger-board preset now Colchester; original profile retains London. Current temperature/conditions, forecast high/low, humidity and wind; a gentle Weather-page animation. Home refreshes weather and shows cached/unavailable states honestly. Location-specific cache prevents London data being reused as Colchester. | [S1, S2]; Colchester live readings/range and widget fit verified on FNK0104N over USB; old-cache compatibility and forecast-date/DST logic checked. After the contrast/moon correction, Dominic confirmed the text and moon work on FNK0104N. |
| APP-02 | Crypto prices | BTC, ETH, SOL and DOGE prices in GBP with 24-hour changes. | [S1]; Dominic confirmed original-board operation; requires internet. |
| APP-03 | Matrix and Game of Life | Offline animations/simulation, with Home navigation. | [S1, S4]; original-board use confirmed. Larger-board navigation confirmed; extended app checks remain. |
| AUD-01 | Internet radio, including BBC World Service | Groove Salad, Drone Zone, Secret Agent and BBC World Service MP3 presets. Play, Stop, speaker Test and saved volume/last played station. | [S1, S3]; decoding/queue checks recorded for BBC and Groove Salad on original board. Larger-board screen retention and BBC decoding checked; decoded samples increased with the display off. Dominic confirmed the test tones and BBC both sounded good. |
| AUD-02 | Practical radio controls | Continues after Home, Stop audio on launcher, 15/30/60-minute sleep timer, bounded reconnect attempts; Voice and Wi-Fi tools stop radio. | [S1, S3]; long playback, outage recovery and sleep expiry need physical acceptance checks. This timer stops radio; it is not an alarm/Pomodoro app. |
| AUD-03 | Voice assistant access | Explicit Voice / Talk entry using the existing XiaoZhi service. | [S1]; requires server activation and microphone/speaker testing. End-to-end operation is **not confirmed**. |
| SYS-01 | More usable touch/settings | Saved touch calibration, brightness, screen off, Home dimming and visible Wi-Fi/battery status. Keyboard clipping/contrast fixes are in source. | [S1, S2]; large-board touch/orientation and redraw corruption were fixed and confirmed clear by Dominic. Backlight saving is not deep sleep. |
| SD-01 | Use SD when available, survive its absence | Optional settings/SSID/password export to `cyberdeck/backup.json`, previous copy retained; internal storage remains primary. | [S6]; original-board write/read-back verified. Larger-board mounting/removal/failure checks remain. **No automatic restore/import.** |
| PWR-01 | Battery percentage | Approximate voltage-based percentage on Home and voltage in Settings; larger board has ADC calibration and an adjustable correction. | [S2, S5]; recorded USB readings about 3.97–4.01 V / 78–82%. This is not a multimeter-validated fuel gauge, charging detection or automatic battery-presence detection. |
| HW-01 | Adapt app to the larger board | Separate FNK0104N profile, larger layout, correct USB-right orientation, working touch and corrected partial redraws; original profile retained. | [S2]; Dominic confirmed upright display, taps and clear status/Apps transition. |
| DOC-01 | Enclosure specification and design-tool handover | Both Markdown briefs exist, with battery/switch/speaker/access requirements and sensor possibilities. | [S7]; **these describe the older 2.8-inch assembly** and need a new larger-board revision before CAD use. |
| DOC-02 | Battery-monitor sourcing and wiring illustrations | SEN0291 researched; UK supplier/cable candidates and plug/solder wiring illustrations supplied. | [S8] and conversation; planning material, not an installed monitor. Cable fit and physical wiring remain unverified. |

## Remaining requested work, in suggested order

| ID / priority | Work item | Status / dependency | Completion criteria |
|---|---|---|---|
| B-01 / High | Copy the original device's saved Wi-Fi credentials to the larger board | **Waiting for credential source.** The requested secret was not found in the scoped searches; the original factory backup predates its Wi-Fi configuration. Do not put passwords in this backlog or Git. | Locate the authorised secret store/file or original device, transfer privately, and demonstrate connection after reboot. Existing larger-board networks remain intact. |
| B-02 / High | Full larger-board acceptance pass | **Partial acceptance.** Display/touch, BBC sound, live Colchester weather and display-off streaming checked. Other networking/SD checks remain. | Check five faces/default persistence; password keyboard; two saved networks/reconnect; Wi-Fi/BLE; weather/crypto; speaker/radio/Stop/sleep timer; startup/offline time; SD backup with missing/failed card. Record each result separately. |
| B-03 / High | Battery calibration, Battery Test screen and portable-power validation | **Backlogged by Dominic; battery physically fitted, runtime screen/logging not implemented.** | Compare voltage with a meter, verify USB charging and battery-only use, implement the Battery Test task below, and confirm honest invalid/unavailable behaviour. Record cell specifications and avoid implying precision from percentage alone. |
| B-04 / High | Charging/discharging indication | **Hardware proposal only; no monitor driver/UI installed.** SEN0291 is the researched candidate, with connector or soldered signal wiring. Direct charger-CHRG modification is an alternative if only a charging flag is wanted. | Confirm chosen hardware and connections; implement reading/calibration, filtering and UI states; test charging, discharging, near-zero current and missing-module fallback. Near-zero current must not be labelled proof of a full or absent battery. Keep existing ADC display usable without the module. |
| B-05 / High | Revise enclosure specification/handover for FNK0104N | **Older-board briefs exist; larger-board packaging not established.** Depends on dimensions of actual PCB, fitted cell, speaker and chosen monitor/switch. | New version with verified dimensions, holes, USB/SD/BOOT/RESET access, antenna clearance, insulated battery support, speaker grille, wiring/strain relief and monitor allowance. No reuse of old 2.8-inch dimensions/pinout. |
| B-06 / Medium | Add a practical physical battery switch | **Candidate researched; purchase/installation not confirmed.** Recheck for the larger board. | Select fitting switch and wiring, verify current rating/polarity, test battery-off and USB/charging behaviour, and incorporate measured switch geometry in enclosure. Do not describe a battery disconnect as a universal USB power switch. |
| B-07 / Medium | Complete enclosure CAD and fit testing | **No fit-tested FNK0104N enclosure established by this repository.** Depends on B-05/B-06 and final hardware choices. | Use revised brief in the design tool; print relevant fit coupons, then a prototype; check screen-edge touch, plugs, speaker, cell and screw clearances before final release. |
| B-08 / Medium | Validate opt-in Voice with microphone and speaker | **Existing integration; not proven end to end.** Requires activation/service choice and working audio. | Explicit user entry starts Voice; test listening, response, Stop/Home and radio interruption without restoring spoken setup prompts at normal boot. |
| B-09 / Medium | Automatically show whether a battery is connected | **Not implemented.** The current fitted-battery setting is manual. Voltage alone, or near-zero SEN0291 current, does not establish presence while USB is attached. | Establish and test a reliable detection method before offering an automatic Connected/Not connected state; otherwise retain an honest manual/unknown state. This may need additional hardware beyond the current monitor proposal. |

### B-03 — Battery Test screen and runtime measurements

Added to the backlog at Dominic's request on **21 October 2026**; this is planned work, not permission to implement or start a discharge test now.

- Add a **Start test / Stop test** screen showing elapsed time, measured battery voltage and approximate percentage.
- Fully charge the battery, record the starting voltage and unplug USB for the timed run. USB power would invalidate a battery-only runtime measurement. The current percentage does not prove charging is complete.
- Periodically save progress and the final result to SD when available, with bounded internal storage as a fallback. Results must remain available after charging/restart; define the maximum progress lost on interrupted power.
- Run three separately charged tests: Home clock with normal dimming, BBC World Service with display off, and radio with display on. Keep brightness/volume fixed and record Wi-Fi state and settings so results are comparable. Disable the radio sleep timer for the radio runs.
- Record the actual cell's capacity/specification and choose a suitable low-battery stopping limit before testing. Save the result at that limit; do not deliberately run the battery until the device dies. Label results as runtime to the chosen limit.
- No extra current-monitor module is required to measure elapsed runtime. Current/charging measurements remain separate work under B-04; no accurate remaining-hours prediction is promised by this task.

Acceptance: saved, recoverable results for all three profiles, including starting/ending voltage, elapsed time and stop reason; verified SD-missing/failure fallback; battery-only operation and the chosen low-battery behaviour checked on the physical device.

## Speaker and sensor ideas discussed — not yet committed features

These came from the requested ideas exploration. Apart from Internet Radio and the startup chime above, they are **ideas awaiting selection**, not claims of installed apps. Suggested first follow-up: a simple offline focus/countdown timer with a gentle completion chime.

| ID | Idea | Current status / smallest useful version |
|---|---|---|
| I-01 | Focus / Pomodoro / countdown | Not implemented; offline work/break or countdown timer with chime and acknowledge action. |
| I-02 | SD music player | Not implemented; browse a supported audio folder, Play/Stop and next/previous. Optional card remains a feature dependency, not a dependency for the whole device. |
| I-03 | Sleep sounds | Not implemented; rain/waves/brown noise with stop timer or fade. Radio's existing sleep timer does not supply these sounds. |
| I-04 | Soundboard / game sound effects | Not implemented; a few large pads for clips, or optional effects for games. |
| I-05 | Talking clock/weather/status | Not implemented; selected information spoken on demand; local phrases or a separately chosen speech service. |
| I-06 | Voice notes | Not implemented; short microphone recordings with explicit recording state and playback, likely on SD. |
| I-07 | Podcasts / spoken-word player | Not implemented; selected feeds and episode playback; downloads/resume are later scope. |
| I-08 | Morse trainer / metronome | Not implemented; local tone generation with simple practice/tempo controls. |
| I-09 | Home-event notifications | Not implemented; authenticated event input, selected chime, mute/quiet hours. External event integration required. |
| I-10 | Audible Wi-Fi/BLE signal finder | Not implemented; beep changes with measured signal. Signal strength must not be presented as accurate distance. |
| I-11 | Audio visualiser / microphone-level display | Not implemented; visualise PCM or relative mic level. A level display would not be a calibrated sound meter. |
| I-12 | More radio stations / station browser / metadata | Not implemented beyond four presets. Evaluate additional stream formats separately; no arbitrary URLs, HLS, AAC or service logins in current Radio. |
| I-13 | Additional sensors | Capabilities researched; no add-on environmental sensor selected/fitted/integrated. Microphone, touch, battery voltage, Wi-Fi and BLE are available inputs. A BME280-style temperature/humidity/pressure module was an example, not a purchase decision. Internal chip temperature is not room or cell temperature. |
| I-14 | Further power saving | Deep sleep/soft power-off discussed as a hardware-design option, not implemented. Existing Home dimming and other-page screen timeout save backlight power only. Measure use cases before choosing sleep/wake behaviour. |
| I-15 | Broader Home customisation | A saved Clock-app favourite exists; selectable boot pages or applying all five faces to Home would be additional scope. Do not silently equate those settings. |
| I-16 | Restore settings from SD | Current feature exports backups only. A deliberate, validated import/migration flow would be additional scope, including treatment of another device's settings. |
| I-17 | Richer Colchester weather card | **Implemented after Dominic selected this next slice.** Current temperature/condition, daily high/low, readable wind/humidity and update time; dated offline cache and background fetch. Location selection remains optional future scope. |
| I-18 | Animated weather | **Implemented after Dominic selected this next slice.** Gentle sun/cloud/rain/snow/fog/storm symbols on Weather, with night clear skies; Home stays calm. Renderer bounds, distinct states and animation checked; long-run battery cost remains unmeasured. |
| I-19 | Duck reaction game / Play section | **Backlogged by Dominic.** A 30-second offline target-tapping game with score/accuracy and optional sound, potentially using the yellow duck. Keep the existing Home navigation; decide whether a Play category is useful as games grow. |

## Earlier Home delivery slice — 21 October 2026

Dominic confirmed **Colchester** and explicitly chose to backlog the richer weather/animation and reaction game while installing the Home controls now. Delivered: three compact icons (headphones for Radio, grid for Apps, power for display off), existing touch-to-wake without activating a control, Colchester weather on FNK0104N and no reuse of old London cache data. Display off is backlight-only, not device shutdown or deep sleep; radio continues. Both board builds, 98 host tests and two native LVGL regressions passed. The 3,363,120-byte larger-board app was hash-verified after installation; USB Home layout/glyph/retention checks passed. **Dominic confirmed all three icons work, including display-off/touch-wake.** Live on-device Colchester weather was subsequently confirmed in the next slice below.

## Weather and field checks — 21 October 2026

Dominic then selected the recommended next slice. Delivered: dedicated Weather layout with current conditions, forecast high/low, wind/humidity, dated update/cache state and a gentle condition animation; Home remains calm. Old current-only caches still load, and old daily ranges are dated rather than labelled “Today”. Forecast downloads and time sync now wait for the Wi-Fi manager's IP-ready state, fixing the early-boot DNS race observed during testing.

Both screen-size builds passed, with 98 host tests and three native tests (touch, redraw and Weather). The 3,367,792-byte FNK0104N image was installed with flash hash verification. USB checks verified live Colchester current/range data, label bounds/order and saved daily range after restart. BBC decoded samples continued increasing while the display was off; Dominic confirmed the tones and BBC both sounded good. A real disconnect event, new IP lease, restored connection and fresh weather verified reconnect to the currently available saved network. Moving between home/office SSIDs remains unverified. Dominic then reported the still moon/dark text. A native reproduction identified inherited dark text (luminance 33 on background 16), corrected to explicit pale text (230). The moon now bobs visibly; its motion regression failed before the fix and passes afterwards. The USB diagnostic now checks text brightness as well as bounds. The 3,368,032-byte corrected image was hash-verified after installation; real-device readability checks passed and Dominic confirmed “Yes, text and moon work”. Long-run power cost remains unmeasured. The focus timer and duck game were not included.

## Dark mode delivery — 21 October 2026

Dominic requested an immediate Dark mode entry in the main Apps menu. Delivered: a Dark mode / Light mode toggle beside Radio and Stop audio, dark/light Home and analogue dial palettes, readable status/weather text, and internal preference persistence without an SD dependency. Selection returns Home; the existing Home icon controls remain, expanded as requested below. Other app pages already use dark backgrounds; individual Clock-app artwork is retained.

The 3,369,792-byte larger-board image was hash-verified after installation. Both board builds, four native tests and sanitized Clock-renderer checks passed. On-device diagnostics clicked the real menu event, verified both menu captions fit, checked dark Home colours/layout, proved dark mode survives restart and restored light mode successfully. Dark mode was left active. Touch/visual acceptance is recorded separately; the Battery Test screen above remains backlog only.

## Latest delivery slice — Home theme and brightness buttons, 21 October 2026

Dominic confirmed both themes work/look good and expanded the request: Home now has Radio, Apps, a moon button for dark/light mode, a sun button for brightness, and display off. The brightness button opens a slider popup with live adjustment from 10–100%, save on release and Done to close; it also closes on navigation. The menu theme entry stays available. Both settings persist internally without SD; brightness remains separate from theme. Settings shares the brightness handler, including a separate save that avoids the backlight's equal-value early return.

Both board builds and four native tests passed. The 3,372,256-byte FNK0104N image was hash-verified after installation. Actual LVGL Home-button event checks passed for both theme directions, all five control bounds and brightness popup fit. Slider value/release events changed 60% to 35%; saved value and backlight output matched, including after reboot. Out-of-range/malformed requests were rejected, then the original 60% was restored and verified. Physical acceptance of the new Home buttons is recorded separately. The Battery Test task remains backlog only.

## Important limits and historical fixes

- The wireless tools are a 2.4 GHz Wi-Fi survey and BLE discovery. No additional radio hardware, NFC, GPS or other specialised tooling has been implemented by this work.
- Battery fitted is a manual setting, enabled by default in the larger profile; the app does not automatically prove a battery is connected. The flashing RGB LED is firmware status, not a charge indicator.
- SD backup contains readable network credentials. Keep cards/backups private and out of Git. The larger profile does not automatically format an unmountable card; original-board one-time formatting permission does not authorise erasing a new card.
- Fixed during development: missed/calibration taps, slow Home response, clipped password keyboard, password text contrast, missing BLE names where advertised, empty Wi-Fi results, Radio returning to menu, spoken activation at boot, larger-board upside-down image and garbled partial redraws. Do not keep these as open bugs unless reproduced again.
- A Wi-Fi-connected indicator means association with an access point, not a successful internet-access check.
- Time needs sync or manual setting after complete power loss. Weather uses dated cached/unavailable states; it is not a local weather sensor.
- The first two plug-wiring image drafts and the first solder-closeup draft were rejected in the conversation. Use only the final files linked under S8, and verify real labels/continuity before wiring; generated illustrations are not PCB layout evidence.

## Evidence and maintenance

Reviewed against firmware **6feda76** (`Add Freenove FNK0104N 3.5-inch cyberdeck support`) and **832e8d9** (`Add ES3C28P touchscreen cyberdeck with wireless tools and apps`), current source and Dominic's confirmations in this conversation. Those firmware commits were previously pushed. The Home/Colchester checkpoint, backlog, project handovers and final wiring illustrations were committed and pushed as **29161d6**. The following Weather update extends that checkpoint.

- **S1:** [Shared app implementation](../main/boards/dominic-es3c28p-cyberdeck/cyberdeck_display.cc) and [app/validation guide](../main/boards/dominic-es3c28p-cyberdeck/README.md).
- **S2:** [Larger-board guide](../main/boards/dominic-fnk0104n-cyberdeck/README.md), [configuration](../main/boards/dominic-fnk0104n-cyberdeck/config.h) and [hardware inspection](cyd-3.5-inspection-2026-10-21.md).
- **S3:** [Radio and local audio](../main/boards/dominic-es3c28p-cyberdeck/cyberdeck_radio.cc).
- **S4:** [Clock renderer](../main/boards/dominic-es3c28p-cyberdeck/cyberdeck_clock.cc), [logic/renderer tests](../main/boards/dominic-es3c28p-cyberdeck/tests/) and [Home concepts](cyberdeck-home-screen-research.md). Research documents are historical proposals; current source/board guide determines delivery status.
- **S5:** [Shared board/network/battery implementation](../main/boards/dominic-es3c28p-cyberdeck/cyberdeck_board.cc); retained network selection also uses the existing `78__esp-wifi-connect` component's SSID manager and station logic.
- **S6:** [Optional SD storage](../main/boards/dominic-es3c28p-cyberdeck/cyberdeck_storage.cc).
- **S7:** [Original enclosure specification](cyberdeck-enclosure-spec.md), [original design handover](cyberdeck-3d-print-handover.md) and [speaker ideas](cyberdeck-speaker-research.md).
- **S8:** Final [plug/adapter wiring illustration](images/fnk0104n-sen0291-wiring.png) and [soldered I²C illustration](images/fnk0104n-sen0291-soldered-wiring.png). No fitted SEN0291 or completed R11 modification is established.

The initial backlog review was documentation/source inspection only. The subsequent Colchester/Home update has the new build/installation checks recorded above and in S2; it did not revalidate every earlier app, charging or battery runtime. Keep item IDs stable; update status and record the board/date/evidence when an item is verified. Do not turn an ideas row into an implementation requirement without Dominic selecting it.
