# Pocket Cyberdeck enclosure specification

Version: 0.1 | Prepared: 4 October 2026 | Status: design brief, not a fit-tested enclosure

Companion: [3D print design handover](cyberdeck-3d-print-handover.md).

## Purpose

Design a compact, comfortable, serviceable enclosure for Dominic's working ESP32-S3 touchscreen cyberdeck, with an internal rechargeable battery and an added slide switch. Use landscape orientation: screen facing the user, microphone at the left, USB-C at the right, as in the supplied photographs.

The device combines Wi-Fi/BLE discovery with Clock, Weather, Crypto, Matrix, Game of Life, Voice and Settings. Touchscreen interaction and direct Wi-Fi joining work. An SD card is installed. Voice/audio and battery operation still need physical validation. Current screen-off behaviour saves the backlight; it is not deep sleep.

## Evidence and component status

| Item | Design basis | Status |
|---|---|---|
| Main board | LCDwiki ES3C28P, 2.8-inch capacitive-touch ESP32-S3 assembly | Matches photographs, pin mapping and working firmware; confirm mechanical revision against physical board |
| Display | ILI9341, native 240 x 320; firmware uses landscape 320 x 240 | Working |
| Touch | FT6336G capacitive controller | Working, calibrated |
| SD card | Existing microSD slot and inserted card | Mounted and settings backup checked |
| Battery | Jauch LP504783JU+PCM+2 WIRES 70MM | Proposed purchase; not installed or electrically qualified on this board |
| Switch | APEM GH36S010001 | Proposed purchase; not installed |
| Speaker | External speaker connected to the board's SPEAKER socket | Exact speaker model, dimensions and mounting method unconfirmed |
| Environmental sensor | Optional future I2C module | None selected or fitted |

Do not substitute dimensions from a generic ESP32 CYD, resistive-touch board or the ES3N28P touchless variant.

## Board geometry

All dimensions are millimetres. Values below come from the [ES3C28P assembly drawing](https://www.lcdwiki.com/res/ES3C28P/ES3C28P_Size.pdf), revision V1.0 dated 11 June 2025, visually checked during preparation. They are manufacturer values, not caliper measurements of Dominic's board.

| Parameter | Value | Interpretation |
|---|---:|---|
| PCB outline, landscape | 86.00 x 50.00 | Does not include attached cable plugs |
| Assembly thickness | 10.60 | Front-to-back manufacturer envelope |
| PCB thickness | 1.60 | Include support clearance below components |
| Rear component height | 4.70 maximum | Measured from PCB rear surface in drawing |
| PCB corner radius | 3.50 | Four corners |
| PCB mounting holes | Four, diameter 3.20 | Nominal M3 clearance; inspect screw-head seating |
| Mounting-hole pitch, landscape | 78.00 x 42.00 | Centres 4.00 from the respective PCB edges |
| LCD active area, landscape | 57.60 x 43.20 | Pixel area |
| LCD visual area, landscape | 58.05 x 43.60 | Drawing's LCD VA; use as minimum reveal basis |
| Touch/display external footprint, landscape | 69.20 +/- 0.20 x 50.00 +/- 0.20 | Distinct from the visible pixel area |

Coordinate convention for enclosure CAD: look at the screen in the intended landscape orientation; origin is the lower-left PCB corner, X increases right, Y increases up, Z increases towards the rear. Set Z = 0 at the frontmost touch-glass plane. Derived PCB hole centres are (4,4), (82,4), (4,46), (82,46). A rear-view drawing is mirrored relative to this front-view convention.

Import the [manufacturer 3D archive](https://www.lcdwiki.com/res/ES3C28P/ES3C28P_3D.zip), containing `ES3C28P_3D/ES3C28P_3D.step`, for component positions and collision checks. Confirm scale, orientation and revision before using it. Do not locate ports from the hole-centre table alone.

The drawing also marks diameter 5.60 features at the mounting locations; do not replace the 3.20 PCB clearance holes with this value. Resolve the associated relief/seating geometry in the STEP model and physical board. The drawing labels touch thickness 1.00, whereas the product page lists 1.20; use the assembly envelope and verify the actual front stack before setting a retaining lip.

## Enclosure layout and construction

Use a front shell/frame and a removable rear cover. Prefer ordinary screws and accessible assembly over a permanent snap fit. Support the board at its mounting locations with insulated standoffs; do not clamp the active glass or touch surface. Use fastener heads/washers that seat without damaging the front assembly. Make case-closure bosses separate from PCB standoffs where required by the battery layout.

Provisional CAD starting choices, adjustable after packaging and fit checks:

| Parameter | Starting choice |
|---|---|
| Outer size | Approximately 100 x 65 x 28; increase if antenna, switch, speaker or wiring needs it |
| Shell wall | 2.0 nominal |
| Outside corner radius | 6.0 nominal |
| Mating clearance | 0.3-0.5 per side, tune for the actual printer |
| Front opening | Start around 59 x 44.5, aligned to the LCD visual area; verify all pixels and edge touches remain accessible |
| Material | PLA for fit prototypes; PETG candidate for the finished case |
| Closure | Screws with captured nuts; final sizes, lengths and boss dimensions matched to chosen hardware |

These are design choices, not measured constraints. The outer size is a target, not a promised fit. Keep the face lip low enough for touches at screen edges; soften grip edges. Include a small lanyard attachment if it fits without compromising the cell or antenna. A kickstand is optional and outside the first fit prototype.

### Openings and access

| Feature | Requirement |
|---|---|
| USB-C, right edge in use | Opening must accept the user's complete cable-plug body and allow full insertion; measure position and plug dimensions |
| MicroSD | Accessible insertion/removal and finger clearance; verify slot direction and whether it is push-push before designing the opening |
| BOOT | Recessed finger access or removable cover access; useful for Home and recovery |
| RESET | Recessed access; avoid accidental operation or a permanently pressed printed pusher |
| Microphone, front left | Short, unobstructed acoustic opening aligned with the actual microphone port |
| Speaker | Grille and mount sized to the selected speaker; protect its diaphragm and separate its acoustic path from the microphone |
| Added slide switch | Thumb-accessible side/top location, with visible ON/OFF indication; avoid the PCB antenna region |
| RGB LED | Optional small viewing opening; no light pipe required for the first enclosure |
| I2C/expansion connectors | Retain internal access when the cover is removed; do not cut external sensor holes before a module is chosen |

Connector coordinates, plug heights, latch clearance, cable bends and button travel must be taken from the matching STEP assembly and checked physically. Do not guess them from photographs. Keep USB accessible for charging, firmware updates and recovery.

## Battery packaging and power

Proposed battery: [Jauch LP504783JU+PCM+2 WIRES 70MM](https://www.digikey.co.uk/en/products/detail/jauch-quartz/LP504783JU-PCM-2-WIRES-70MM/21725020), a protected single-cell 3.7 V pack, 2100 mAh typical / 2050 mAh minimum, with wire leads rather than the board's mating plug. Listing dimensions are 84.5 x 47.5 x 6.0. The manufacturer drawing uses 85.0 maximum length; reserve a conservative **85 x 47.5 x 6.0 pack envelope** and confirm the exact supplied variant. See the [Jauch pack datasheet](https://jauch.com.pl/files/productscatalogs/S82Rf9sj_lp504783ju_1s1p_2_wire_70mm.pdf).

Start with an unobstructed battery bay of **87 x 50 x 8**, plus a separate allowance for the wire exit, protection-board/tab region and connector. This is a design allowance, not a battery swelling specification. The manufacturer maximum envelope and final pack measurements govern. Use a smooth insulating tray and gentle retention without compressing the pouch. The cell must be replaceable; keep screw tips, sharp ribs and solder joints out of its volume. Select screw lengths so they cannot reach the cell.

The cell nearly spans the PCB. A centred stack can collide with board mounting bosses and put its foil pouch behind the printed antenna. Solve this explicitly: offset the cell into an extended rear pocket or revise the battery choice. Keep the antenna's immediate volume clear of battery foil, metal fasteners, speaker magnet and wiring. Determine the exclusion region from board geometry and validate radio performance; no measured keepout distance is established in this brief.

The board's BAT socket is documented as two-pin, 1.25 mm pitch. Pitch alone does not identify the connector family or polarity. Verify the mating housing and BAT+ / BAT- before making an adapter. Use the board's existing charger only after checking compatibility with the actual pack: Jauch specifies 4.2 V charging, while LCDwiki's page lists 4.24 V saturation. This discrepancy and charging behaviour need electrical verification before portable use. No battery runtime is yet measured.

Simple power arrangement:

```text
Battery positive -> slide-switch common
Slide-switch selected throw -> board BAT positive
Other throw -> unused, insulated
Battery negative -> board BAT negative
USB-C -> board's existing USB-C socket
```

The [board schematic](https://www.lcdwiki.com/res/ES3C28P/2.8inch_ESP32-S3_Display_Schematic.pdf) indicates that this switch disconnects the battery from both load and charger. Expected behaviour: battery-only OFF stops battery supply; USB can still run the board with the switch OFF; charging requires ON. This has not been tested on the physical battery-equipped board. The switch is therefore a battery disconnect, not a universal USB-and-battery power switch.

Allow ventilation and separation between the charger area and cell; do not seal the enclosure or claim water resistance. Chip-temperature sensing does not measure cell temperature.

## Slide-switch geometry

Candidate: [APEM GH36S010001](https://www.digikey.com/en/products/detail/apem-inc/GH36S010001/1796125), single-pole, two-position ON-ON, solder-lug panel mount. Wiring one throw gives the intended ON/OFF function. The [APEM G-series datasheet](https://www.apem.com/storage/datasheets/files/PCB_slideswitches_serie_G/media/PCB-slideswitches-serie-G.pdf), pages 2-4, provides:

| Parameter | Nominal value |
|---|---:|
| Panel actuator opening | 6.50 x 4.20 |
| Mounting-hole pitch | 17.53 |
| Mounting-hole diameter | 2.40 |
| Mounting plate length | 19.50 |
| Standard H actuator projection | 5.10 |
| Slider travel | 2.20 |
| Silver-contact rating | 3 A at 30 VDC, resistive load |

Use the single-pole `GH36S0` body drawing, not the wider double-pole `GH46S0` drawing. Add solder-lug, insulation and wire-bend clearance. A thick printed wall may need an internal recess to keep the actuator reachable; check plate thickness and screw/nut clearance using the physical switch. Make a small switch-mount test coupon before printing the full case.

## Onboard inputs and information

Hardware basis: [LCDwiki interfaces and pin allocation](https://www.lcdwiki.com/2.8inch_ESP32-S3_Display) and this repository's board profile.

| Input | Information it can provide | Current implementation / limit | Enclosure implication |
|---|---|---|---|
| MEMS microphone via ES8311 audio path | Voice/audio; future relative sound-level display or sound-triggered actions | Audio path configured, physical audio check pending; sound meter not implemented; not calibrated dBA |
| Battery ADC, GPIO9 / ADC1 channel 8 | Battery voltage and approximate charge level | Settings estimate configured with assumed 200k/200k divider; calibration and battery test pending; no current, fuel-gauge or cell-temperature measurement |
| FT6336G capacitive touch | Touch position and interaction timing | Working; no pressure/force measurement |
| 2.4 GHz Wi-Fi radio | Nearby SSIDs, channel, signal strength and security mode | Survey working; signal is not reliable distance or exact location |
| BLE radio | Nearby advertisements, advertised names, addresses and signal strength | Active scan working; names depend on advertisements; interpreting remote sensor payloads would require additional firmware |
| ESP32-S3 internal temperature sensor | Chip-temperature trends | Not exposed in the current menu; unsuitable as a room or battery thermometer |
| BOOT button | Manual input | Home/recovery functions; not an environmental sensor |

[Espressif documents](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/temp_sensor.html) that the temperature sensor measures inside the silicon and should not be used for ambient temperature.

No onboard GPS, accelerometer/gyroscope, compass, ambient-light, humidity or air-pressure sensor has been identified. Screen, RGB LED and speaker are outputs; the SD card is storage. Weather currently comes from an internet service using fixed London coordinates, not local sensing or detected location.

For a useful first sensor addition, consider a **3.3 V-compatible BME280 breakout** for local temperature, humidity and pressure. [Bosch documents these measurements and I2C support](https://www.bosch-sensortec.com/en/products/environmental-sensors/humidity-sensors-bme280). The board exposes shared I2C SDA16/SCL15 with 3.3 V/GND. Verify connector pin order, bus address, pull-ups and voltage before attaching a module; do not confuse this with the UART socket's 5 V pin. New firmware is required. Put any ambient sensor in a vented location away from processor/charger heat, potentially outside the main enclosure. Leave its mechanical attachment optional until a breakout is selected.

## Required design-tool outputs

1. Editable, parameterised CAD source and STEP assembly with component envelopes, separate case pieces and a dimension table.
2. STL or 3MF for each printable part, with units and suggested print orientation/material.
3. Exploded assembly preview, fastener/part list and brief assembly instructions.
4. Small first-print fit pieces for the PCB/front opening, USB/SD access and switch mount.
5. A list of unresolved dimensions and collision checks, distinguishing nominal source values from measured and proposed values.

## Fit and acceptance checklist

- Confirm the actual board matches the STEP model; measure outline, thickness, holes and front seating.
- Resolve exact USB, SD, BOOT, RESET, microphone, LED and connector positions, including fitted plug envelopes.
- Confirm purchased battery envelope, wire exit, connector/polarity and charger compatibility; confirm switch body and speaker geometry.
- Verify there are no intersections among cell, bosses, PCB components, switch lugs, speaker and wires, and protect the antenna region.
- Check front reveal, edge touches, plug insertion, card removal, button travel and slider travel on test prints.
- Assemble without glass/cell pressure or pinched wires; inspect screw-tip clearances and service access.
- Compare Wi-Fi/BLE scans with the enclosure open and closed; check charging heat and battery monitoring physically.
- Record printer, material, nozzle/layer settings and actual fit adjustments before calling the enclosure print-ready.

## Project and photograph references

- Firmware repository: [dotalbot/ESP32_S3](https://github.com/dotalbot/ESP32_S3), firmware baseline `832e8d914dd4a19731560125ac9cd8b06b7e1397`.
- Board implementation and validation: [board guide](../main/boards/dominic-es3c28p-cyberdeck/README.md).
- Local reference photographs: `/Users/dominic/Downloads/IMG_5766.jpeg` (rear), `IMG_5770.jpeg` (BAT/USB detail), `IMG_5771.jpeg` (rear antenna/expansion), `IMG_5772.jpeg` and `IMG_5773.jpeg` (front landscape orientation). These photos are not bundled here; attach them if the design tool cannot read that directory.
- Manufacturer board reference: [ES3C28P documentation](https://www.lcdwiki.com/2.8inch_ESP32-S3_Display), [mechanical drawing](https://www.lcdwiki.com/res/ES3C28P/ES3C28P_Size.pdf), [STEP archive](https://www.lcdwiki.com/res/ES3C28P/ES3C28P_3D.zip), [schematic](https://www.lcdwiki.com/res/ES3C28P/2.8inch_ESP32-S3_Display_Schematic.pdf).

This specification authorises a parameterised enclosure draft. It does not establish that the proposed shopping parts fit, that battery charging is qualified, or that a finished enclosure has been tested.
