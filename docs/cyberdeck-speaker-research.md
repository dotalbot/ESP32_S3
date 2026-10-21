# Speaker apps for the portable cyberdeck

Prepared: 4 October 2026. Status: ideas and feasibility research; no new audio firmware installed.

## Direction

Make this a pocket radio, music player and useful audio companion. Start with Internet Radio, then reuse the same playback controls for SD music, focus timers and sleep sounds. The touchscreen should keep large Play/Stop, volume and Home controls; music can continue after returning Home, with a visible playing indicator and an easy Stop action. These are proposed behaviours, not current features.

## What is established

The ES3C28P has a microphone, ES8311 audio codec, speaker amplifier, external speaker connector and microSD storage. Treat the attached speaker as one mono output. Speaker model, impedance, enclosure acoustics, playback and microphone operation still need physical checking. [Manufacturer hardware documentation](https://www.lcdwiki.com/2.8inch_ESP32-S3_Display)

The current firmware already owns I2S playback through `AudioService`, using Opus decoding and a playback queue. The board configures 24 kHz mono output. Its manifest includes `esp_audio_codec ~2.5.0`, and local `sdkconfig` enables MP3/AAC decoders; this does not implement radio. `PlaySound()` currently parses Ogg/Opus. Audio and battery tests remain outstanding. [Repository audio design](../main/audio/README.md), [board status](../main/boards/dominic-es3c28p-cyberdeck/README.md), [dependency manifest](../main/idf_component.yml), [playback implementation](../main/audio/audio_service.cc)

## Ideas worth developing

Effort is a relative planning estimate after the shared audio path works. Every row below is a proposed app.

| Idea | Experience | Needs | Effort |
|---|---|---|---|
| **Internet Radio — first** | Favourite stations, station name, volume, reconnect and sleep timer | Wi-Fi; compatible stream | Medium |
| **SD music** | Browse a small music folder; next/previous and shuffle | SD; supported MP3/WAV files | Medium |
| **Focus / Pomodoro** | Work and break countdowns; gentle completion chime | Offline; embedded sound | Low |
| **Sleep sounds** | Rain, waves or brown noise with a fade-out timer | Offline; synthesis or SD loops | Low–medium |
| **Soundboard and game effects** | Large pads for personal clips; sounds for existing games | Offline; flash or SD | Low–medium |
| **Talking clock / weather** | Read the time, weather or device status aloud | Pre-recorded phrases, or a TTS backend; weather needs Wi-Fi | Medium |
| **Voice assistant** | Use the existing XiaoZhi Voice app once speaker and mic are checked | Wi-Fi; activation/server; mic | Existing path, untested hardware |
| **Voice notes** | Record, name and replay short notes | Mic and SD; visible recording indicator | Medium |
| **Podcasts / spoken-word player** | A few chosen feeds, episode list and later resume support | Wi-Fi; optional SD downloads | Higher |
| **Morse trainer / metronome** | Tap-to-learn Morse; rhythmic practice | Offline; generated tones | Low |
| **Home notifications** | Doorbell, washing-machine or sensor event plays a chosen chime | Wi-Fi and an external event source | Medium |
| **Audible signal finder** | Map changing Wi-Fi/BLE signal strength to beep speed or pitch | Radio scans; no internet | Low–medium; signal is not reliable distance |
| **Audio visualizer** | Music spectrum or relative microphone-level display | Decoded PCM or mic samples | Medium |

HTTP/file playback and channel/sample-rate conversion are documented by [ESP Audio Simple Player](https://components.espressif.com/components/espressif/esp_audio_simple_player/versions/1.1.0/readme). Recording is plausible from the existing microphone path, but requires a new recorder; microphone level is not a calibrated sound-pressure measurement. Notifications require a new authenticated local endpoint or automation integration, with mute and quiet hours.

## Internet Radio: a manageable first version

Proposed MVP: 3–6 presets; Play/Stop; volume; Connecting/Playing/Reconnecting status; current station; 15/30/60-minute sleep timer. Remember volume and favourite station in internal storage, with optional SD backup. Keep basic radio independent of the card. Start with MP3; add AAC after tests. Do not automatically play on startup.

Proposed screen flow: Home > Radio > choose a station > Now Playing. Keep station name, large Play/Stop and volume controls visible; put favourites and sleep timer on a second control view if the 320 x 240 screen becomes crowded. Show song information only when a station supplies usable metadata. Returning Home should keep radio playing and display a small Now Playing/Stop control. Screen-off should keep playback running until Stop or the sleep timer expires.

Use one shared volume control for these apps, reusing the existing saved audio volume. For the first release, selecting Voice should stop radio explicitly; do not attempt simultaneous voice and music. A timer should interrupt playback with a local chime and an on-screen acknowledgement. Keep a simple Stop action available from every audio screen. These policies are proposals to settle before coding, not behaviours already installed.

SomaFM publishes direct streams intended for hardware players. Suggested first stations:

| Station | Starter MP3 endpoint |
|---|---|
| Groove Salad | `https://ice5.somafm.com/groovesalad-128-mp3` |
| Drone Zone | `https://ice5.somafm.com/dronezone-128-mp3` |

These endpoints were found in the official lists, not playback-tested here. SomaFM recommends its stable playlists because direct servers can change; a later version can resolve playlists or keep fallback servers. [Groove Salad streams](https://somafm.com/groovesalad/directstreamlinks.html), [Drone Zone streams](https://somafm.com/dronezone/directstreamlinks.html)

Station websites are not stream URLs. Handle redirects, HTTPS validation, unknown-length responses and reconnects. If requesting ICY metadata, remove metadata before decoding; defer song-title display until this is tested. ESP-IDF supplies HTTP streaming and TLS support; ICY handling is additional application work. [HTTP client](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/protocols/esp_http_client.html), [audioI2S source example](https://github.com/schreibfaul1/ESP32-audioI2S)

Defer HLS, login-protected services and streams requiring expiring URLs. BBC presets need individual endpoint and availability checks; international access differs by service. [BBC access information](https://help.bbc.com/hc/en-us/articles/42652680528659-Update-on-access-to-BBC-Sounds-outside-the-UK)

## Engineering choices and later additions

Stay with ESP-IDF 6.1. Evaluate the existing `esp_audio_codec` simple decoder first: it accepts fragmented MP3/AAC input. Alternatively, ESP Audio Simple Player provides HTTP/file playback and PCM callbacks. Integrate through the existing output owner, including stereo-to-mono and sample-rate conversion. Its current release is a candidate requiring a dependency/build check, not proven compatible with this checkout. [Codec documentation](https://docs.espressif.com/projects/esp-adf/en/latest/basic-components/audio-codec/esp-audio-codec.html)

`esp_audio_render` has an official network-decode-to-I2S example if mixing becomes necessary. Avoid adding a second independent I2S driver. Arduino `ESP32-audioI2S` demonstrates radios, SD playback and metadata; use it as inspiration rather than importing its Arduino stack. [Render example](https://components.espressif.com/components/espressif/esp_audio_render/versions/0.7.1/examples/audio_render?language=en)

A later station browser can filter Radio Browser results by codec, bitrate and HLS status; directory inclusion does not guarantee playback. Podcasts use RSS media enclosures; seeking/resume and HLS require more than the simple player. [Radio Browser API](https://docs.radio-browser.info/), [RSS specification](https://www.rssboard.org/rss-specification), [Espressif player comparison](https://docs.espressif.com/projects/esp-techpedia/en/latest/esp-friends/solution-introduction/multimedia/component-description/esp-player.html)

Do not assume ordinary Bluetooth-speaker/A2DP, Spotify or AirPlay support. The S3 radio is Bluetooth LE; consumer music services need separate protocol/service investigation. [ESP32-S3 datasheet](https://documentation.espressif.com/esp32_s3_datasheet_en.pdf)

Before implementation: test a low-volume speaker tone and voice playback; measure free memory; define interruption between radio, Voice and alarms. Wi-Fi survey currently disconnects briefly, so it will interrupt radio unless coordinated. Playback needs bounded buffering, responsive Stop/Home, reconnect limits and an audible-alarm fallback that works without Wi-Fi/SD. Battery runtime remains unmeasured. Start with radio plus a local timer chime; add the remaining apps incrementally.
