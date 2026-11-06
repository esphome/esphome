# sigma_delta speaker and rtttl sample taps

## Pulse density of the speaker pin, measured inside the chip (ESP32-P4, #18702)

Nothing is wired to the pin. `scripts/sigma_delta/sdprobe.hpp` calls `gpio_input_enable()` on the SDM output pin,
and a core 0 task reads it with `gpio_get_level()` 2000 times per window, with pseudo-random gaps of 0 to 255 cycles
(xorshift) that decorrelate the samples from the 1 MHz modulator. One window is about 2 ms and the density noise is
about 0.5% sd. The expected duty for an int8 density d is (d + 128) / 256, so 16-bit PCM at +50% reads 75%, 0 reads
50%, and full scale reads 99.6% or 0%.

The same header has:
- a core 0 feeder task (DC, sine, stereo L/R, a burst of N frames, any 8/16/24/32 bit mono or stereo format) that
  calls `speaker->play(..., 20 ms)`;
- a played-frame counter on `add_audio_output_callback()`: frames per second over 4 s matches the rounding of the
  10 MHz timer exactly;
- a CPU probe: a priority 10 task pinned to the loop core spins for 200 ms, and the iterations while playing against
  idle give the interrupt share (44.1 kHz cost 20% of core 1).

`gen.py tpl.yaml > NAME.yaml` builds the whole sequence (the device name comes from `esphome -s name NAME`): DC levels,
volume, mute, formats, 2 Hz and 50 Hz sines with DUMP lines, sample rates, underrun, stop mid-play, finish, rtttl and a
play/stop hammer. `gen2.py` (both share the step helpers in `steps.py`) builds the inverted pin run (set the speaker
pin to `{number: GPIO21, inverted: true}` in a copy of `tpl.yaml`) plus the CPU cost per rate. `run.sh LABEL
[gen|gen2]` generates, builds, flashes, captures with `--ms` until `ALLDONE` and prints the `RES` lines; for `gen` it
then runs `an.py`, which fits the sines and prints the level edges of the underrun, stop and finish dumps. GPIO21 is
free on the Waveshare ESP32-P4 Module DEV KIT. Long operation warnings in the log come from the probe's own lambdas.

## rtttl click measurement (#22075)

`scripts/rtttl_edge_tap/edgetap.hpp`: `EdgeTap`, a pass-through `speaker::Speaker` that forwards to the real speaker
and reads the int16 samples rtttl writes. `rtttl.set_speaker()` it from a lambda, and call `sync()` from an
`interval: 10ms`, because `is_running()` and `is_stopped()` read the base class state. Use tunes with a rest between
every note, so a note is a run of samples between runs of 8 or more zeros. It logs each note's onset and release
level (the largest of the first or last 4 samples as a share of the note's peak): dev about 95%, the 32-sample fade
at most 12.5%. `rtttl-edge-tap-dut.yaml` is the AUX-2 config with a USB speaker as the mixer output (it needs the
AUX-2 package next to it, see [usb_audio.md](usb_audio.md)); `run.sh LABEL` plays eight tunes and prints the note
reports. Any speaker works the same way: wrap the one rtttl writes to.
