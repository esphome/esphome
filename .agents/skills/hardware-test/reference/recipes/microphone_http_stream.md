# microphone_http_stream (#21593)

`microphone_http_stream` serves raw microphone PCM at `/audio/microphones/<stream_id>` (and a listing at
`/audio/microphones`) on the web server. `templates/microphone_http_stream/`:
- `mh-base.yaml`: Waveshare ESP32-S3-ETH on PoE (logs over the API on Ethernet, the native USB port is the USB host
  port), encrypted API, `web_server` on 80 with a dummy sensor and switch, mic start and stop buttons and a line
  every 5 s with the mic state and internal and PSRAM heap. Any ESP32-S3 with PSRAM works for `mh-fake.yaml`.
- `mh-fake.yaml`: the stream fed by `ext/fakemic`, a test microphone that publishes 32 ms chunks of a 16-bit counter
  (one count per sample) from its own task, so a reader can check every sample. Its "Mic stall" button stops the
  chunks for 6 s.
- `mh-usb.yaml`: the stream fed by a USB microphone on `usb_audio` (48 kHz, 16-bit stereo, channel 0 streamed).

Copy the directory next to a `secrets.yaml` with `api_key`, flash, then run
`mhclient.py HOST STREAM_ID [--clients N] [--seconds S] [--pattern] [--stall K] [--stagger S] [--expect-reject R]`.
Each reader prints its HTTP status, format headers, sample count and rate, the longest gap between reads and, with
`--pattern`, every break in the counter. `--stall K` makes reader K stop reading after 2 s while keeping its socket
open, then checks that the device closed it. `--expect-reject R` marks the last R readers as past `max_clients`;
they must get a non-200 status or a close or reset before any samples, and a timeout or a served reader fails.
The script exits 1 when the listing is not a 200 naming the stream, the stalled reader was not dropped, a reader past
the limit was not refused, or any other reader got no 200, no samples, an early EOF, a read error or a broken counter.

Cases worth running on each head: one reader with `--pattern` (rate must match the sample rate, 0 breaks), the
`max_clients` limit plus one with `--expect-reject 1` (note what the extra one gets; the others must be unaffected), a stalled reader among
three (the device should drop the stalled one only), the "Mic stall" button during a read (a gap of about 6 s, then
the counter continues), mic start and stop while readers are attached, and the heap line after many connect and
disconnect cycles. On the USB microphone, check the rate and `rms` instead of the pattern with `--rate 16000 --min-rms 1`, and add `--max-gap-ms` to fail on long gaps; these set the exit code.
