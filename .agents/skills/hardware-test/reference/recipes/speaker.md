# speaker

## Media player pipeline tasks (#21203)

The `speaker` media player platform (`speaker/media_player/`) creates `ann_read`, `ann_decode`, `med_read` and
`med_decode` FreeRTOS tasks on every play and deletes them after a stop. The AUX-2 and sendspin packages use the
`speaker_source` player instead, which has its own pipeline and never runs that code, so a test of
`speaker/media_player/*` has to swap the player: `templates/aux-2-preprod-speaker-player-dut.yaml` removes the
package's `external_media_player` with `id: !remove` and adds a `speaker` platform player on the package's resampler
speakers, plus a "Task names" button whose lambda calls `xTaskGetHandle()` on the four names and logs internal free
heap. Serve FLAC files from the host (`ffmpeg -f lavfi -i "sine=frequency=440:duration=90" -ac 2 -ar 48000 -c:a flac
media90.flac`, a 3 s mono one for announcements, `python3 -m http.server 8765 --directory www`); `audio_http`
refuses WAV and MP3. `scripts/speaker_media_player/cases.sh HOST KEY MEDIA_URL ANN_URL CAPTURE LABEL` runs three
cycles through `drive.py` (`play:URL | ann:URL | stop | press:NAME | wait:S` on one API connection) with the probe
pressed idle, during media, during the announcement, after it and after the stop. Pass: idle none, media `med_*`
only, announcement all four, after stop none, every cycle. API commands land a second or two late over Wi-Fi, so read
the probe against the `State changed to` lines rather than the driver's timestamps. Internal free heap after a stop
moved by 2.5 kB between two boots of the same build, so a heap delta needs more than one boot per build. Results:
https://github.com/esphome/esphome/pull/21203#issuecomment-6299581943

## Streamed TTS with a pause (#21090, #21091)

Home Assistant streams TTS while it is still being generated, so the player must start on the first part instead of
waiting for the whole file. `scripts/speaker_tts_pause/`:
- `mk.py INTRO_AUDIO CONT_AUDIO www/s1.flac` builds one 48 kHz mono FLAC stream (the intro padded to a 4096-sample
  frame boundary, then the continuation) and writes the byte offset of the split to `s1.flac.cut`. Make the intro
  with any TTS, e.g. `say -o intro.aiff "..."` on macOS; build a short one (`s1`) and a long one (`s2`).
- `srv.py PORT www` serves the file chunked: the header and intro at once, then it holds the connection open for
  `PAUSE` seconds (8), then the rest.
- `tts-common.yaml` swaps the AUX-2's `speaker_source` player for the `speaker` platform player and counts played
  frames with `add_audio_output_callback` on both resampler speakers into the counters in `frames.h` (one `[frames`
  log line per change, read over serial with `--ms`). Its `on_boot` must be a list entry: a dict `on_boot` replaces
  the package's list and the AUX-2 never starts Wi-Fi or Ethernet.
- `run.sh LABEL=TREE...` flashes each build and drives two short-intro and two long-intro announcements and one
  media play through `scripts/speaker_media_player/drive.py` (set `DRIVE` when running from a scratch copy); `an.py`
  prints first audio, the server's send times and how much of the intro played before the rest arrived, per request.
  A request fails when it never played the whole intro or stopped short of 95 % of the stream.

Before the two PRs the player waited for the rest of the stream (8.2 s) for both a 22.5 KB and a 50 KB intro; with
both it starts at about 1 s.

## Voice PE wake word audio (#21885)

`scripts/voice_wake_word_audio/va.py HOST KEY OUTDIR accept,reject,timeout` stands in for Home Assistant
(aioesphomeapi 46.27.0 or later, `wake_word_audio=True`), answers each run by mode and saves both channels as WAV;
`locate.py OUTDIR` finds the played phrase in them by envelope cross-correlation against `wake16k.wav`.
`run.sh HOST TAG [ROUNDS]` runs reject and timeout rounds while playing the phrase every 9 s, starting once `va.py`
is READY. `va.py` fails a START before READY or during a run, and `run.sh`, `stale.py` and `mww.py` fail at once
when the player fails or hangs for 30 s.

- Give the test build its own API key so Home Assistant cannot hold the voice assistant subscription.
- Make the phrase with any TTS (`say -o wake.aiff "Okay Nabu"`, then `ffmpeg -i wake.aiff -ac 1 -ar 16000
  wake16k.wav`) and play it from the host near the device (`PLAY`, default `afplay`). On macOS the terminal needs
  audio permission first; before that `afplay` hangs.
- The PE mute switch only mutes samples. To stop microWakeWord, add `mww-buttons.yaml` (template buttons calling
  `micro_wake_word.stop` and `start`); `mww.py` checks that no run starts while it is stopped.
- Stale clip check: `stale.py` says a spoken marker word (`marker.aiff`), stops microWakeWord for 3 s, starts it and
  says the wake word; `stalean.py` measures loud audio before the phrase in each clip and exits 1 above
  `MAX_LOUD_MS` (default 100). The loud level has no default: calibrate `LOUD_LEVEL` on each setup from the phrase
  and pre-phrase peaks it prints (a run without it prints them and fails), somewhere between room noise and the
  marker's peak. A trial whose phrase peak is under twice `LOUD_LEVEL` fails as too quiet to judge. A steady 1 kHz
  beep never reaches the PE stream (the XMOS processing removes it), so the marker has to be speech.

## FLAC stream changes without a DAC (#22878)

`scripts/flac_stream_changes/`: `flac-dut.yaml` is a Waveshare ESP32-S3-ETH `speaker` media player (resampler, mixer
and `i2s_audio` on spare pins GPIO16/17/43 with no DAC attached, Ethernet only) with frame counters on both resampler
speakers and a 1 s heap probe line. No DAC or speaker is needed: the counters show what was played. `run.py HOST
BASEURL LABEL [formats|switch|leak]` (API_KEY in the env) plays each format to the end, sends 12 URL changes mid play,
6 stop and play rounds, 5 announcements over music and 20 full plays; serve the FLAC files it names with any HTTP
server. The files are not in the repo; make them from any source clip with ffmpeg. The name gives channels (`s`
stereo, `m` mono), sample rate in kHz and FLAC block size (`b4096`), `_24` is 24-bit; for example
`ffmpeg -i src.wav -t 10 -ac 2 -ar 44100 -c:a flac -frame_size 4096 s44b4096.flac`, `-c:a libmp3lame` for `s44.mp3`
and plain `s44.wav`. Use about 10 s per file, 30 s for `long.flac` and 2 s for `ann.flac`. `an.py CAPTURE` counts errors and crashes and summarises the heap, `plat.py CAPTURE` prints the frame deltas
between idle periods.

- The 20 full plays plus the last play of the switch phase sum to an exact frame count for a given file set, so two
  builds compare exactly.
- The speaker media player rejects 24-bit audio on dev (16-bit only).

## FLAC decode benchmark on the device (#22990)

`scripts/flac_stream_changes/flacbench.hpp` (an `esphome: includes:` header) decodes four FLAC clips embedded in
flash with `.incbin` through the same calls as `AudioDecoder::decode_flac_()`: `RAMAllocator` input and output
buffers, output resized after `HEADER_READY`. It runs in its own task (core 1, priority 5), times only `decode()` and
logs `RES r<round> <file> ... rtf X fnv H ... used_dram/used_iram/used_ps` (what the decoder took from each heap),
then `BENCHDONE`. Set the clip directory with `-s media /absolute/path` (the configs pass it as the `FB_MEDIA` build
flag). The config needs `audio: codecs: flac:`; `buffer_memory: internal|psram` picks the micro-flac Kconfig.

- `bench-esp32.yaml` runs five rounds 3 s after boot on an ESP32 with no network (serial log). `bench-s3.yaml` is
  `flac-dut.yaml` plus a "Flac Bench" button (`bench-button.yaml`); `flow.sh NAME YAML TREE yes|no` (env `HOST`,
  `BASEURL`) uploads it over the network, logs over the API with `apilogcap.py`, runs the `run.py` flow when asked
  and then `PRESSES` bench presses; `NOFLASH=1` and `KEEP=1` append to the capture.
- Clips: `tools_extract.py HEADER OUT` pulls the public domain orchestral FLACs out of micro-flac's
  `examples/decode_benchmark/main/test_audio_flac*.h`; cut 3 s with ffmpeg (mono with `-ac 1`) and encode with
  `flac -5 -b 4096` into `s16`, `m16`, `s24` and `m24.flac`. Reference hashes: decode with
  `flac -d --force-raw-format --endian=little --sign=signed` and take FNV-1a over the bytes, one `name length hash`
  line per clip. `sumbench.py HOST_FNV CAPTURE...` checks every run against them; `cmp.py PR BASE` gives the PR
  against base median and best real-time factor.
- `plat.py CAPTURE --exact` gives the final play plus 20 plays frame count from the `run.py` flow probes without
  reading plateaus by hand.
- Noise: an ESP32-PICO-D4 with no network in the config repeats to 0.05 %; an S3 on Ethernet with PSRAM varies a
  few percent per run, so pool about 45 runs per build over alternating boots. On micro-flac 0.3.1 an ESP32
  without PSRAM put 32 KB of decoder buffers in IRAM (16-bit stereo 16.2 times real time); 0.3.2 puts them in DRAM
  (34.1 times).
