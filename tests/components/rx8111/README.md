# RX8111 time-only PR 1 hardware test

`time_bench.yaml` is a **destructive** automatic test for the dedicated classic ESP32
Adafruit Feather HUZZAH32 and RX8111CE. It does not load the RX8111 binary or
text sensor platforms. It rewrites the RTC time to **2026-01-31 23:59:57 UTC**
and modifies alarm, timer, EVIN, FOUT, and flag registers. Every reboot repeats
the test. Do not run this firmware on the Freezer Monitor ESP32-C3. The HUZZAH32
fixture is used for testing, not as a deployed clock or monitor.

Keep the Feather powered by **USB independently of the RTC /INT line**. Connect
grounds and keep all signals at 3.3 V. **Remove any other driver from EVIN**
before connecting GPIO18; never connect two push-pull outputs together. Check
the actual pinout and power/charging arrangement before flashing; charging must
remain disabled when using a non-rechargeable battery.

| RTC signal | HUZZAH32 pin |
| --- | --- |
| SDA | GPIO23 |
| SCL | GPIO22 |
| /INT | GPIO4 (input pull-up, active LOW) |
| EVIN | GPIO18 (test-driven 3.3 V) |
| FOUT | GPIO19 (PCNT input) |

From `/home/test/Documents/GitHub/esphome`, with the bench connected:

```sh
/home/test/.cache/tmp102-review-venv/bin/python -m esphome config tests/components/rx8111/time_bench.yaml
/home/test/.cache/tmp102-review-venv/bin/python -m esphome compile tests/components/rx8111/time_bench.yaml
# Replace /dev/ttyUSB0 with the port actually detected on this machine.
/home/test/.cache/tmp102-review-venv/bin/python -m esphome run tests/components/rx8111/time_bench.yaml --device /dev/ttyUSB0
```

On October 14, 2026 the PR 1 worktree after clang-tidy cleanup was flashed to the HUZZAH32 and
reported `PASS=17 FAIL=0 SKIP=3` in 163 seconds. This is a test of that
worktree, not of any later edits. The serial log is
`/tmp/rx8111-hardware-tidy-cleanup.log`. This dedicated test fixture does not
need normal firmware or trusted RTC time restored after a bench run.

Observe serial output until the `RESULT:` line (allow several minutes for the
1/60 Hz timer and 1 Hz FOUT windows). Long phases log `WAIT:` progress every
five seconds with elapsed time, deadline and current counts. Report **PASS,
FAIL and SKIP counts**, and any phase-level messages. The time-only runner checks RTC time read/write,
invalid weekdays, clock/calendar rollover, fixed and relative alarms and flag
clearing, four timer sources and repeat intervals, EVIN/EVF/INT and raw latest
timestamp capture, charging-off readback, and measured FOUT frequencies. GPIO
level polling does not prove absence of narrow INT glitches. Battery switchover,
retention, voltage thresholds, charging, other EVIN settings, and long-term
accuracy remain **unverified** without additional equipment. The runner uses
raw registers for timer/EVIN/FOUT changes where no runtime action exists; it
does not independently exercise every static configuration path.
The absence of short `/INT` glitches and the battery voltage thresholds are
electrical characteristics, not software-only acceptance criteria. Correct
interrupt flag handling and register mapping for backup and EVIN options are
still component responsibilities; the bench verifies only the combinations
it actually runs.

The bench leaves the RTC at a synthetic date and disables RTC wake interrupts;
that is intentional for this dedicated test fixture. Do not treat a compile or
simulator run as a physical PASS. Record the tested worktree, framework, wiring
and results so the tested firmware is identifiable.

Native action/schema tests:

```sh
RX8111_BUILD_TESTS=1 /home/test/.cache/tmp102-review-venv/bin/python -m pytest -q -o addopts= tests/components/rx8111/test_rx8111.py
```
