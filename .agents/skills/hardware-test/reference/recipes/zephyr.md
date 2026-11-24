# zephyr

## Zephyr preferences on the nRF52840 dongle (#20810)

`templates/nrf52-preferences-dut.yaml`: logger on USB_CDC at VERBOSE, `flash_write_interval: 5s`, one
`globals` with `restore_value` incremented every 15 s. Give each build its own global id so each first boot
sees a never-saved key (settings survive a reflash). Flow per build: flash through the log port, 40 s capture
of the first boot, then start a second capture and reboot with a 2001 baud touch on the dongle's other CDC
port (the dfu rate callback is global, and the capture holds the log port); `serial_capture.py` reopens the
log port after the USB re-enumeration and gets the buffered boot lines (`load setting`, `Loaded N settings`).

## nRF54L preferences and debug info on the XIAO nRF54LM20A (#22839, #22840)

`scripts/nrf54_xiao/`: `pref.yaml` (restored `counter`, `boots` and a per build soft reboot flag; boot 1 waits 30 s,
syncs and calls `App.safe_reboot()`), `prefflow.sh NAME PORT` (build and flash with `pio_build.sh`, 100 s capture of the
boot and the soft reboot, then 40 s around a `pyocd reset -m sw`, events in `NAME-events.txt`), `dbg.yaml` (every debug
entity logged from `on_value`) and `dbgw.yaml` (a busy loop after a software reset, to force a watchdog reset). Keep the
global ids the same across builds so a backend change shows whether old keys are read. An `esphome` upload (pyOCD
`-e sector`) keeps the settings partition across reflashes. Check the backend in `.pioenvs/<name>/zephyr/zephyr/.config`
(`CONFIG_SETTINGS_*`).

- NVS (dev before #22840) works on the nRF54L RRAM. NVS to ZMS loses all values silently (`Loaded 0 settings.`). ZMS
  to NVS hangs the NVS build at boot after `Log initialized` (PC in `nvs_prev_ate`/`nvs_read_hist`); reflash a ZMS
  build to recover.
- The reset reason is never cleared on Zephyr, so causes accumulate (`Software reset, Watchdog timer expiration, Debug
  event`); a `while` on a reason string from the debug text sensor loops forever. `CPU Frequency` reads 1 MHz (GRTC).
- Debug values on the XIAO (#22839): part code 0x54bc20a, variant AAB0, RAM 512 kB, RRAM 2036 kB, main regulator
  DC/DC, POF disabled; only UARTE20 (the logger) enabled.

## zephyr_pwm duty and frequency without wires (#22849)

`scripts/nrf_pwm_probe/`: `probe.hpp` reconnects the input buffer of a pin the PWM peripheral drives (`PIN_CNF.INPUT`),
polls `IN` for 20 ms with IRQs locked and times edges with the DWT cycle counter (375 ns per sample on the
nRF54LM20A, about 330 ns on the nRF52840); `sweep.yaml` steps every output through 0, 10, 25, 50, 90 and 100 % twice
and logs one `MEAS` line per pin and step. `x54.yaml` (XIAO nRF54LM20A: LEDs P1.22, P1.24 and P1.23 active low, P3.04
is D15) and `i52.yaml` (ItsyBitsy nRF52840: LED P0.06, P1.02, A0 P0.04, A1 P0.30, A2 P0.28). Flash `x54.yaml` with
`esphome run`; `i52run.sh NAME` flashes the ItsyBitsy by serial DFU and captures.

- Results matched the set duty within 0.001 and the frequency within 0.05 Hz.
- PWM pins: the nRF54L15 allows P1 only, the nRF54LM20 P1 and P3 (the PWM20 to PWM22 power domain). A pin on another
  port needs its own PWM block; the nRF52840 has four blocks, so four distinct frequencies at most.
