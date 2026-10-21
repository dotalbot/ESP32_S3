# Handover to 3d_print_design: Pocket Cyberdeck

Prepared: 4 October 2026 | Brief version: 0.1 | Requested output: parameterised enclosure design and fit prototypes

Use this document as the design-tool prompt. The detailed companion is [cyberdeck-enclosure-spec.md](cyberdeck-enclosure-spec.md). Critical constraints are repeated here so this handover also works on its own.

## Task

Create a compact, rounded, landscape handheld enclosure for Dominic's working 2.8-inch ESP32-S3 capacitive-touch cyberdeck. Provide a front shell and screw-removable rear cover, internal battery tray, added slide-switch mount, USB-C access, SD access, microphone opening and provision for an external speaker. Use practical FDM geometry with minimal supports. Prefer serviceability and a straightforward first print.

There is no dedicated board power switch apparent in the supplied photos. Add an external switch; do not design around an imaginary onboard switch. In use, the microphone is at the left of the screen and USB-C is at the right. Preserve this orientation.

The firmware offers Wi-Fi survey, BLE discovery, Clock, Weather, Crypto, Matrix, Game of Life, Voice and Settings. Touch, networking and the internet apps work. SD backup works and the card is optional. Battery operation and audio are not yet physically verified. Backlight timeout is not deep sleep.

## Board dimensions and authoritative CAD

Target **LCDwiki ES3C28P capacitive-touch** assembly, subject to confirmation against the physical board. Do not use a generic CYD or the ES3N28P touchless model.

All units are mm. Manufacturer drawing values:

| Feature | Dimension |
|---|---:|
| PCB, landscape | 86.00 x 50.00 |
| Complete assembly thickness | 10.60 |
| PCB thickness | 1.60 |
| Rear component maximum height from PCB | 4.70 |
| PCB corners | R3.50 |
| Four PCB mounting holes | Diameter 3.20 |
| Hole pitch | 78.00 x 42.00 |
| Edge-to-hole-centre offset | 4.00 |
| LCD visual area, landscape | 58.05 x 43.60 |
| LCD active area, landscape | 57.60 x 43.20 |
| Touch/display footprint, landscape | 69.20 +/- 0.20 x 50.00 +/- 0.20 |

Front-view CAD coordinates: lower-left PCB corner is (0,0); X right, Y up; Z = 0 at frontmost glass, increasing rearwards. Hole centres derived from drawing: (4,4), (82,4), (4,46), (82,46). Rear-view features must be transformed correctly, not copied without mirroring.

- [Manufacturer dimensional drawing](https://www.lcdwiki.com/res/ES3C28P/ES3C28P_Size.pdf), V1.0, 11 June 2025.
- [Manufacturer STEP download](https://www.lcdwiki.com/res/ES3C28P/ES3C28P_3D.zip): archive contains `ES3C28P_3D/ES3C28P_3D.step`. Import it to establish ports, component locations and clearances. Confirm units/revision against the physical board.
- [Board documentation](https://www.lcdwiki.com/2.8inch_ESP32-S3_Display).

Do not invent connector coordinates. The drawing marks separate diameter 5.60 mounting-location features; these are not replacements for the diameter 3.20 PCB holes. Touch thickness differs between drawing and webpage (1.00 versus 1.20); verify the seating stack physically.

## Proposed battery: keep it replaceable in CAD

Candidate **Jauch LP504783JU+PCM+2 WIRES 70MM**, protected 3.7 V single cell, 2100 mAh typical / 2050 mAh minimum. Not purchased/installed as far as this handover establishes. It has wire leads and needs the correct board mating connector.

- [DigiKey exact candidate](https://www.digikey.co.uk/en/products/detail/jauch-quartz/LP504783JU-PCM-2-WIRES-70MM/21725020).
- [Jauch pack datasheet](https://jauch.com.pl/files/productscatalogs/S82Rf9sj_lp504783ju_1s1p_2_wire_70mm.pdf).
- Reserve **85 x 47.5 x 6.0** for the pack, pending final supplied-part measurements.
- Starting clear bay **87 x 50 x 8**, with additional wire-exit/PCM/connector allowance. This is a provisional clearance, not a manufacturer swelling limit.
- Use an insulating tray, smooth surfaces and gentle removable retention. No clamp pressure, screw tips or sharp ribs against the pouch.
- Do not send PCB mounting bosses through the cell volume. Put closure screws outside it.
- The battery is almost the PCB's full length: check antenna overlap and mounting conflicts. Offset the battery into a larger rear pocket or revise the battery choice if necessary.

BAT is documented as a two-pin 1.25 mm-pitch socket; connector family and polarity require confirmation. Battery charging requires verification: pack specification is 4.2 V, while the board webpage lists 4.24 V saturation. Do not declare electrical compatibility or runtime from this brief alone.

## Proposed switch: keep the mount replaceable in CAD

Candidate **APEM GH36S010001**, single-pole ON-ON slide switch, solder lugs, panel mount, silver contacts rated 3 A at 30 VDC for resistive load. Not installed.

- [Exact part listing](https://www.digikey.com/en/products/detail/apem-inc/GH36S010001/1796125).
- [APEM G-series datasheet](https://www.apem.com/storage/datasheets/files/PCB_slideswitches_serie_G/media/PCB-slideswitches-serie-G.pdf): panel opening page 2, part code page 3, single-pole GH36S0 body page 4.
- Nominal actuator opening **6.50 x 4.20**; mounting holes **diameter 2.40**, **17.53** pitch; mounting plate length **19.50**; H actuator projection **5.10**; travel **2.20**.
- Use the single-pole body, not the wider GH46S0 double-pole dimensions.
- Leave space for insulated solder lugs, screw/nut hardware and bent wires. Recess the inner wall if needed for actuator reach. Make a fit coupon.

Place the switch on a convenient top/side edge away from the printed antenna. Mark ON/OFF according to the actual wiring. Battery positive runs through common and one throw; the remaining throw is insulated. Battery negative goes directly to BAT negative. This is a **battery disconnect**: according to the [board schematic](https://www.lcdwiki.com/res/ES3C28P/2.8inch_ESP32-S3_Display_Schematic.pdf), USB can still power the board when OFF and charging requires ON. Battery-equipped operation has not yet been tested.

## Mechanical requirements

- Starting outer envelope approximately **100 x 65 x 28**, wall **2.0**, outer corner **R6.0**. These are adjustable design targets, not established fit dimensions.
- Start mating clearances at **0.3-0.5 per side**, then tune to the actual printer/material. PLA fit prototype; PETG candidate final material.
- Mount PCB on insulated standoffs using its four mounting positions. Keep forces off the glass, active touch surface and components; verify head seating.
- Start front reveal around **59 x 44.5**, align using STEP geometry, and confirm every pixel and edge touch is accessible. Keep bezel lip shallow.
- Provide USB-C opening for the complete cable plug; keep full insertion and firmware-recovery access.
- Provide SD insertion/ejection and finger clearance, based on the actual slot mechanism.
- Provide recessed BOOT/RESET access; avoid permanent button preload.
- Align an acoustic hole with the microphone at the front-left. Reserve a parameterised speaker mount/grille; exact speaker geometry is unresolved, so do not make up a fixed diameter.
- Keep the printed antenna's immediate region free of battery foil, speaker magnet, metal fasteners and routed wires. Determine geometry from STEP/board; radio-test the assembled layout.
- Include separate acoustic paths for microphone and speaker, wire channels, strain relief and removable rear access to BAT/SPEAKER/I2C connectors.
- Allow charger ventilation and avoid heating the cell directly. Do not claim a sealed or waterproof enclosure.
- Optional lanyard hole is useful; kickstand and extra sensor compartments can wait until the basic fit works.

## Sensors and future expansion

Existing hardware can provide microphone audio, battery-voltage estimates, touch position, Wi-Fi survey readings, BLE advertisement readings and ESP32-S3 internal chip temperature. Audio and battery sensing need physical validation; chip temperature is not currently in the menu and is not room/cell temperature. There is no identified onboard GPS, IMU, compass, ambient-light, humidity or air-pressure sensor. The existing Weather app uses internet data and fixed London coordinates.

Leave service access to shared I2C **SDA16/SCL15, 3.3 V/GND**. A future 3.3 V-compatible BME280 breakout could supply local temperature/humidity/pressure, with added firmware. No module is selected; do not fix holes or a compartment to hypothetical dimensions. Ambient sensing would need a vented, thermally isolated position away from processor/charger heat.

## Inputs still required before a final fit release

1. Physical-board match to manufacturer model; actual connector positions/heights, cable envelopes and button travel.
2. Purchased battery envelope, wire exit/PCM region, mating connector/polarity and charging verification.
3. Physical switch body, mount hardware and panel fit.
4. Selected speaker diameter/outline, thickness, magnet, mounting and lead routing.
5. Printer, material and fastener choice; measured fit-coupon adjustments.

Proceed with an editable draft using the source dimensions and explicit placeholders. Report unresolved measurements; do not label a full STL print-ready until they are resolved. Prefer small fit prints first.

## Deliverables and checks

Deliver editable parametric CAD, STEP assembly with component envelopes, separate printable STLs/3MFs, exploded preview, dimension/fastener list, assembly instructions and print orientation. Include small front/PCB, port-access and switch fit coupons.

Check collisions and tolerances; glass/cell loading; screw-tip clearance; cable insertion; SD removal; BOOT/RESET/slider travel; service access; microphone/speaker paths; and antenna clearance. After physical assembly compare open/closed-case Wi-Fi/BLE performance and test charging temperature and battery readings. Report actual measurements separately from assumptions.

## Local context

Repository: [dotalbot/ESP32_S3](https://github.com/dotalbot/ESP32_S3), working firmware baseline `832e8d914dd4a19731560125ac9cd8b06b7e1397`; board profile `dominic-es3c28p-cyberdeck`.

Local photos useful to attach: `/Users/dominic/Downloads/IMG_5766.jpeg`, `/Users/dominic/Downloads/IMG_5770.jpeg`, `/Users/dominic/Downloads/IMG_5771.jpeg`, `/Users/dominic/Downloads/IMG_5772.jpeg`, `/Users/dominic/Downloads/IMG_5773.jpeg`. They are not bundled with this handover. Preserve landscape microphone-left / USB-right orientation.

Firmware settings remain in internal storage; optional SD backups contain readable network credentials. Do not include card contents, passwords or firmware backups in CAD deliverables.
