# i2s_audio

## I2S microphone loopback on one S3 (#22130)

With no I2S microphone at hand, `scripts/i2s_loopback/` makes one. `dcgen.hpp` runs an ESP-IDF std TX channel on
I2S1 as master (GPIO4 BCLK, 5 WS, 6 DOUT, 16-bit stereo) in its own task, switching patterns (DC step, opposite DC
per channel, DC plus a 1 kHz tone, full and half scale 50 Hz squares) through `dcgen::set_mode()`. Jumper 4 to 15,
5 to 16 and 6 to 17 to an `i2s_audio` microphone on I2S0 with `i2s_mode: secondary`. `dcgen::on_data` (registered
with `add_data_callback`) keeps per-segment and last 500 ms stats plus a 1 ms trace of channel 0, and `report()`
logs `SEG` and `SETTLE` lines from an interval. `run.sh LABEL [SAMPLE_RATE] [CHANNEL]` flashes and captures 60 s,
and `sum.py` tabulates. The segment min and max include the tail of the previous pattern (capture latency), so judge
with the tail values. The runner fails on a missing segment, a full-scale square under rms 8000 (no signal through the
jumpers), on either channel for a stereo run, or a crash after the first segment.
Results: https://github.com/esphome/esphome/pull/22130#issuecomment-6615554179
