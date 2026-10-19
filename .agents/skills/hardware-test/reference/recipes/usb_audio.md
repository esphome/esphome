# usb_audio

## USB audio and the usb_host FIFO split on the Waveshare ESP32-S3-ETH (#21081, #11728, 2026-10-18)

Board on PoE, first flash by the native USB port, then the port is the USB host port and the Anker PowerConf S330
speakerphone goes in; logs over the API on Ethernet (`logger: hardware_uart: UART0` so the USB JTAG console does
not fight the OTG port). `templates/usb-audio-waveshare-s3-eth-dut.yaml`: `usb_audio` hub, microphone 48 kHz
16-bit stereo, speaker 48 kHz 24-bit stereo (the S330 speaker offers only 24-bit), buttons for capture, a 1 kHz
beep fed from an 8 ms interval with `play(buf, len, 0)` (never block the loop: a 50 ms wait per chunk while the
speaker is still starting held the loop 10 s and the task watchdog reset the board), stop buttons, and a 5 s line
with mic bytes, speaker state and heap. Drive with `scripts/homeassistant_light/drive.py IP KEY press "Beep"`.
Expect 192 kB/s mic bytes, the tone is audible, split RX 80 / NPTX 16 / PTX 104 in `defines.h`. Control: a
throwaway worktree with `_reconcile_fifo()` returning at once gives the balanced split and
`HCD DWC: EP MPS (294) exceeds supported limit (128)`; the component then retries the speaker start every 70 ms
without end, even after `speaker.stop`. Results: https://github.com/esphome/esphome/pull/21081#issuecomment-6254750828
and https://github.com/esphome/esphome/pull/11728#issuecomment-6254751055

Retest of 3683023 (2026-10-18): a case runner that OTAs a build, presses Beep, Mic start, Beep again and the two stop buttons, and greps the API log does OTA, beep, mic start,
beep, stop and prints the device lines; variants `pr16/` (speaker 16-bit, format rejection path) and `ctl/`
(reconcile no-op). Both earlier findings fixed: one start failure, no retries, rates listed once. Results:
https://github.com/esphome/esphome/pull/11728#issuecomment-6254994926

Music Assistant target (`templates/usb-audio-sendspin-s3-eth-dut.yaml`, the working one): copy the Apollo
CAST-1 wiring exactly. `speaker: platform: mixer` (inputs `timeout: never`, output the 24-bit `usb_speaker`,
`task_stack_in_psram: true`) fed by two `resampler` speakers (48 kHz, 16-bit), `sendspin:` hub with
`unpaired_access: true` and `task_stack_in_psram: false`, `media_source:` sendspin plus `audio_http` entries, and
`media_player:` sendspin (controller) plus `speaker_source` whose media pipeline lists the sendspin source and an
HTTP source and whose announcement pipeline has its own HTTP source. A `media_player: platform: speaker` wired to
the mixer is not a media-source consumer: Music Assistant streams, the player logs `over its buffer; dropping
items`, no audio. Music Assistant found the device by mDNS (`_sendspin._tcp`) and completed the Noise
handshake within 15 s of every boot without pairing.

Judge the chain from the device log, not only by ear: the sendspin sync task prints `Lost sync (N us off)` /
`Regained sync` (DEBUG, tag `SsSync`) for every correction over 5 ms, and a positive error means it inserted that
much silence. Head 3683023 alternated two sizes summing to 96 ms about 10 times a second (callback stamped with the
start time of a whole `play()` batch); head d3be642 (finish-time stamp) left one ~90 ms correction every 1 to 4 s,
rated 99 % by ear. A `usb_audio` speaker `buffer_size` of 28800 (100 ms at 24-bit stereo) with
`write_timeout: 50ms` sounded better than 9600 on both heads. To hear the chain without the sync task, serve a FLAC
over plain HTTP from the host (`python3 -m http.server`, `ffmpeg -i tone.wav -c:a flac tone.flac`) and start it on
the `speaker_source` player with `scripts/usb_audio/playurl.py HOST KEY URL`; the `audio_http` decoder refuses
MP3 and WAV (`Could not determine audio file type`) and HTTPS hosts fail to connect. A ~89.5 ms correction size is 2^32 us at 48 kHz, so a 32-bit `frames * 1000000`; to tell whether it is the
speaker's queue estimate, drop `buffer_size` below 4294 frames (19200 B at 24-bit stereo) and recount: on
2026-10-18 the rate stayed at 64 to 91 per 210 s, so the wrap was upstream (mixer + resampler hold ~4800
frames). Results:
https://github.com/esphome/esphome/pull/11728#issuecomment-6255247327 and
https://github.com/esphome/esphome/pull/11728#issuecomment-6256868046

Instrumenting sendspin-cpp on the bench: after the first compile edit
`.esphome/build/<name>/managed_components/sendspin__sendspin-cpp/src/sync_task.cpp` (SS_LOGD lines in
`process_playback_progress()` and before the late-chunk skip in `decode_chunk()`), then update only that file's
`size` and sha256 `hash` entry in the component's `CHECKSUMS.json` and compile again. Leave `.component_hash` and
the `dependencies.lock` entry at the registry hash: deleting the hash file fails the next full configure, and a
recomputed hash makes the manager refetch the pristine component or refuse the lock. Check the edit is in the image
with `strings firmware.ota.bin | grep <log text>` before flashing. The Apollo CAST packages set the framework
`log_level` to ERROR, which compiles the library's ESP_LOG lines out, so merge `esp32: framework: log_level: DEBUG`
in the test file. The chunk lead (`client_ahead - playtime_ahead`) histogram is the number to compare between
speakers: below -5 ms the library skips the whole chunk.

Nabu Casa Connect AUX-2 (a pre-production unit): build from `packages/aux-2-pre-prod.yaml` at
NabuCasa/aux-2 commit 283f2e64 (dropped in bde41e01), not the public `aux-2.yaml` (different W5500, codec and I2S
pins; the public one reads W5500 chip ID 0x00 and fails the PCM5122 I2C writes). Redirect its local
`external_components` source to `github://NabuCasa/aux-2@main`. Templates `aux-2-preprod-stock-dut.yaml` and
`aux-2-preprod-usb-audio-dut.yaml` (usb_audio speaker as the mixer output via `!extend mixing_speaker`). Console is
the CH343 (1a86:55d3); first flash over it, OTA after. The package switches itself to Ethernet-only on link detect.
Both outputs gave 0 corrections and a -0.09/0.01 ms lead on 2026-10-18.
