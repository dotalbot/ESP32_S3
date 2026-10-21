# Connected 3.5-inch CYD: initial inspection

Date: 21 October 2026. Scope: identify the connected hardware without installing or changing firmware.

## Verified on USB

| Item | Observed value |
|---|---|
| Port | `/dev/cu.usbmodem1101` |
| USB interface | Espressif USB Serial/JTAG, VID `0x303a`, PID `0x1001` |
| Device identity | `b8:1f:3f:c5:29:a0` — different from the original board |
| Processor | ESP32-S3 QFN56, revision v0.2, dual core, 240 MHz |
| Embedded PSRAM | 8 MB, AP_3v3; reported by esptool, not a RAM stress test |
| Flash | 16 MB, manufacturer `0x5e`, device `0x4018`, quad flash, 3.3 V |
| Crystal | 40 MHz |
| Existing application metadata | `arduino-lib-builder`, version `bb76cb1`, ESP-IDF `v5.4.1-1-g2f7dcd862a-dirty` |
| Application build date | 28 March 2025; this is firmware metadata, not a board manufacture date |

Read-only inspection used esptool 5.4.0 `--no-stub flash-id` and two 4 KB flash reads. Bootloader access briefly reset the device. No flash writes, erases, credential changes or battery GPIO probes were performed. A 12-second serial capture received ROM/startup output but no useful display or battery telemetry. The application metadata does not identify the board manufacturer or exact model.

Partition table read from `0x8000`:

| Partition | Offset | Size |
|---|---|---|
| nvs | `0x9000` | `0x5000` |
| otadata | `0xe000` | `0x2000` |
| app0 | `0x10000` | `0x300000` |
| app1 | `0x310000` | `0x300000` |
| ffat | `0x610000` | `0x9e0000` |
| coredump | `0xff0000` | `0x10000` |

The app0 descriptor was read; the currently selected OTA slot was not established. Raw partition/header reads are temporary diagnostics under `/tmp/cyd-3-5-*-2026-10-21.bin`, not a complete firmware backup.

## Reported by the user, not independently measured

- Display: 3.5 inches, 320 × 480 pixels (480 × 320 when used in landscape).
- A battery is attached.

## Assessment and remaining identification

The processor and reported memory are suitable candidates for porting the existing cyberdeck apps. The requested screen has twice the pixel count of the original 320 × 240 landscape screen. Existing display layout, touch mapping and board pins need a separate board profile; the original 2.8-inch firmware must not be assumed compatible.

The initial USB-only inspection did not identify the exact model or peripheral wiring. The user subsequently supplied “3.5inch LCD Display, ESP32-S3 320x480 IPS C-Touch Freenove”; the official-source follow-up below matches that description to FNK0104N. Battery percentage, actual charging, runtime and battery-only operation still have not been physically tested. Preserve a complete, private, checksummed firmware backup before any future installation.

Project-memory skills were consulted. The current workspace's memory pages contained historical 2.8-inch information, not identification of this connected board. The routing index returned no ESP32 entry, and no matching dedicated bank was found in the configured server's bank inventory. No sources were imported or memory configuration changed; findings are retained here in the firmware repository.


## Freenove model match and official-source follow-up

The user identified the board as Freenove 3.5-inch ESP32-S3 320x480 IPS capacitive touch. Freenove's [model table](https://docs.freenove.com/projects/fnk0104/en/latest/fnk0104/codes/MAIN/Freenove_ESP32S3_Display.html) and [official repository](https://github.com/Freenove/Freenove_ESP32_S3_Display) identify the matching family as **FNK0104N**, with **ST77922** display controller. This is a manufacturer-description match, not a chip-level LCD identification. Do not substitute the ST7796 configurations for similarly sized FNK0103/FNK0102 boards or the 4-inch FNK0104S.

The official [3.5-inch schematic](https://github.com/Freenove/Freenove_ESP32_S3_Display/blob/main/Schematic/3.5inch_ESP32-S3_Display_Schematic.pdf) was downloaded and inspected visually, including enlarged battery and MCU regions. The repository tree returned untruncated; model-specific source was inspected directly from its examples and `Libraries/FNK0104N/TFT_eSPI_v2.5.43.zip`. The setup-selection ZIP also includes defaults for other models, so its unselected ESP8266 pin definitions must not be mistaken for this board's wiring.

| Function | Manufacturer schematic / FNK0104N source |
|---|---|
| Display | ST77922, 320x480 portrait / 480x320 landscape, QSPI |
| QSPI | CS10, clock12, D0=11, D1=13, D2=14, D3=9; manufacturer's driver uses SPI2_HOST, 80 MHz |
| Backlight | GPIO41 |
| Touch | Manufacturer's separate `ST77922_TOUCH` driver, I2C address `0x55`; SDA38, SCL39, reset48, interrupt47 |
| Battery sensing | GPIO8, ADC1 channel7; R14/R15 each100k, 1:1 divider, cell voltage approximately twice measured ADC voltage |
| Charger | TP4054, programmed by3.3k resistor; shared USB/battery power circuitry shown in schematic |
| Charging-state telemetry | Schematic does not route charger CHRG to an ESP32 input; do not promise a software charging flag based on voltage alone |
| Audio | ES8311 codec and SC8002B amplifier; enable GPIO1; shared I2C SDA38/SCL39 |
| Audio I2S | MCLK17, BCLK18, codec->MCU16, MCU->codec15, WS21 |
| SDMMC | CLK5, CMD4, D0=6, D1=7, D2=2, D3=3 |

The [official battery example](https://github.com/Freenove/Freenove_ESP32_S3_Display/blob/main/Tutorial_With_Touch/Sketches/Sketch_05.1_Battery_Voltage/Sketch_05.1_Battery_Voltage.ino) explicitly selects GPIO8 for FNK0104N and multiplies calibrated ADC millivolts by2. Its default compile-time model is the 2.8-inch variant; select the FNK0104N branch when using it. GPIO9 is display QSPI D3 on this board, so the old cyberdeck battery pin must not be reused.

The [manufacturer preface](https://docs.freenove.com/projects/fnk0104/en/latest/fnk0104/codes/MAIN/Preface.html#battery-optional) documents lithium-battery operation and slow USB charging, a nominal3.7-4.2V battery input and an MX1.25mm connector. Its images cover the product family; confirm physical connector/polarity and the fitted cell's own label rather than assuming capacity or protection from the product description. Battery chemistry, capacity, measured voltage and runtime of this actual pack remain unknown.

Assessment: the existing apps are candidates for reuse, including Radio/BBC World Service, clock faces, weather, wireless discovery and the duck startup. A separate FNK0104N board configuration is needed for QSPI, address0x55 touch, changed audio/SD/battery pins and the480x320 layout. Existing FT6336 address0x38 touch code cannot be assumed compatible. Battery voltage/estimated percentage is supported by the documented sensing circuit, but needs live validation and calibration with the fitted cell. No new firmware was built or installed during this follow-up.

## Subsequent authorized implementation — 21 October 2026

The user subsequently authorized the port. A complete 16,777,216-byte factory backup was saved privately and SHA256-verified before any flash write. The separate `dominic-fnk0104n-cyberdeck` profile was then built and installed; see its [implementation and validation guide](../main/boards/dominic-fnk0104n-cyberdeck/README.md). Thus the earlier inspection-only statements above describe the preceding stage, not the current installation.

The installed Home screen passed the USB widget-fit/retention check at 480 × 320. The calibrated ADC reported approximately 3974 mV and 78% with the fitted battery and USB attached; this is an estimate, without multimeter or discharge validation. The user confirmed correct display rendering and reported incorrect touch positions. A native regression using actual LVGL input rotation reproduced the double-rotation error; the corrected coordinate pipeline passed every screen pixel. Physical verification of the corrected taps is tracked in the board guide.

The user subsequently confirmed that touch seemed to work, but `IMG_5855.jpeg` established that the image was upside down with USB on the right. That mounting orientation is now explicit: mirror both panel axes and normalize both touch axes before the existing calibration. The native regression covers this physical orientation as well as touch alignment; the board guide records installation and physical verification status.
