# usb_audio

## USB audio on the Waveshare ESP32-S3-ETH (#21081, #11728)

`templates/usb-audio-waveshare-s3-eth-dut.yaml`: ESP-IDF, PoE, logs over the API on Ethernet (the native USB port is
the host port, so there is no console), `usb_audio` hub, a 48 kHz 16-bit microphone, a 48 kHz 24-bit speaker (many
speakerphones offer only 24-bit; a wrong format logs the device's alternates), buttons for capture start and stop,
a non-blocking beep fed from an 8 ms interval with `play(buf, len, 0)` (a blocking `play()` on the main loop trips
the task watchdog), and a status interval with mic bytes, speaker state and heap. Drive the buttons with
`scripts/homeassistant_light/drive.py IP KEY press "Beep"`. Expect 192 kB/s microphone bytes and the FIFO split
RX 80 / NPTX 16 / PTX 104 in the build's `defines.h`; with the balanced split the 294-byte OUT pipe is refused
(`EP MPS (294) exceeds supported limit (128)`). A device whose OUT endpoint is larger than 600 bytes cannot be
claimed on an S3 at all.

## Music Assistant through sendspin

`templates/usb-audio-sendspin-s3-eth-dut.yaml` overlays the template above with the Apollo CAST-1 wiring: resampler
speakers into a mixer whose inputs never time out, `speaker_source` media player with the sendspin source, and
`sendspin: unpaired_access: true`. A `media_player: platform: speaker` is not a media-source consumer and gives
"Player over its buffer", no audio. Music Assistant finds the device by mDNS within 15 s of boot.

Read the result from the device, not only by ear: sendspin's sync task logs `Lost sync (N us off)` (DEBUG, tag
`SsSync`) for every correction over 5 ms, a positive N inserts that much silence, and each one is an audible click.
For the chunk lead histogram, edit the managed library in the build directory,
`.esphome/build/<name>/managed_components/sendspin__sendspin-cpp/src/sync_task.cpp` (an `SS_LOGD` before the
late-chunk skip in `decode_chunk()` printing the chunk's lead against `new_audio_client_playtime`), update only that
file's `size` and sha256 `hash` in the component's `CHECKSUMS.json`, and compile again; deleting `.component_hash` or
recomputing it makes the manager refuse or refetch. Check the edit is in the image with `strings firmware.ota.bin`.
A lead below -5 ms makes the library skip the whole chunk; an I2S speaker keeps it within 0.1 ms of zero, which is
the figure a USB speaker has to match.

Gotchas: wait two minutes after a reboot before the measured stream (safe mode's boot counter write stalls the cores
for about 140 ms once, 60 to 75 s after boot); with the serial console on, a DEBUG log line per report saturates
UART0 and trips the task watchdog, so use `logger: baud_rate: 0` and the API log; two overlapping `esphome logs`
captures double every count; and every audio test must include a pause and resume cycle, since a stream end that
leaves less than one transfer parked in the driver was what hung the speaker after #11728 merged.

## Controls

Apollo CAST PRO-1 (`templates/cast-pro-1-sendspin-control.yaml`): `Core.yaml` next to it is
`Integrations/ESPHome/Core.yaml` from ApolloAutomation/CAST_PRO-1, which also defines the `version` substitution.
Keep its framework log level at ERROR (DEBUG floods driver output and trips the watchdog); the package reboots itself
into Ethernet-only once it sees a link and appends the MAC suffix to the name. `wifi: enable_on_boot: false` keeps
sendspin off Wi-Fi.

Nabu Casa Connect AUX-2 (a pre-production unit): build from `packages/aux-2-pre-prod.yaml` at NabuCasa/aux-2 commit
283f2e64 (dropped in bde41e01), not the public `aux-2.yaml`, whose W5500, codec and I2S pins differ (the public one
reads W5500 chip ID 0x00 and fails the codec's I2C writes). Point its local `external_components` source at
`github://NabuCasa/aux-2@main`. Templates `aux-2-preprod-stock-dut.yaml` (I2S codec) and
`aux-2-preprod-usb-audio-dut.yaml` (the USB speaker as the mixer output via `!extend mixing_speaker`). The console is
the CH343 (1a86:55d3); first flash over it, OTA after. Forcing the Wi-Fi flag off with `restore_value: no` boot-loops
when Ethernet fails, so let the package switch itself.
