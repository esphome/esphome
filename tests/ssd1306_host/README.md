# SSD1306 host validation

Run `python script/test_ssd1306_partial.py` from the repository root.
Use `--compiler clang++` for a second compiler. Logs and CSV benchmarks are kept
in the printed temporary directory; `--output-dir PATH` selects another location.
ASan and UBSan are enabled by default. The runner fails rather than silently
omitting them. `--no-sanitizers` is an explicit fallback, not equivalent coverage.

The runner compiles the actual `ssd1306_base.cpp`, `ssd1306_i2c.cpp` and
`ssd1306_partial_update.cpp` sources. The headers under `support` are small test
substitutes for allocation, GPIO, time, logging, the display base and the I2C bus.
They are `.h.txt` templates so ordinary firmware and C++ unit-test builds cannot
accidentally include them. They are copied into the runner's temporary directory.
This validates the component's control flow, not the whole ESPHome runtime or
physical SSD1306 timing. Clang's warning about the unchanged legacy GNU C++ VLA
remains visible but is not promoted to an error in this standalone runner.

`test_partial_update_planner.cpp` is also an ordinary GoogleTest source for
`python script/cpp_unit_test.py ssd1306_i2c`. That route uses the real repository
host framework and GoogleTest. Its test manifest includes only the pure planner;
the firmware matrix still compiles the full driver. The standalone runner uses
a minimal assertion adapter to run that same planner test source offline.

## Checks

- Eight planner test groups: 65,536 exhaustive sixteen-column masks, every single
  byte in all six supported geometries, 6,000 seeded random cases, gap/chunk
  boundaries, empty/reused plans, bad dimensions, and forced workspace overflow.
  Coverage and transfer cost are checked independently of the production helpers.
- Twenty driver test groups: required/optional allocation failure, allocation
  order, initialization faults, late display-on failure, brief power loss, warning lifetime,
  data-only resync, retry escalation, reset timing, outage probing, timer wrap,
  requested settings/power state, unchanged frames, and legacy behavior.
- 112 legacy configurations (14 models, eight parameter sets) match transaction
  traces from PR commit `46c70b4f338bfb2987de77cc45edaca8453a82a5`. They are checked
  both with the feature compiled out and with it compiled in but disabled for
  the instance. `legacy_baseline.txt` contains SHA-256 digests of those original
  traces. Do not update it simply to silence a failure.
- Nine reproducible synthetic workloads compare the production planner with one
  rectangle per dirty page and one global dirty rectangle. The CSV distinguishes
  modeled I2C clocks from measured host CPU time. No bus, clock stretching,
  driver overhead, rendering time or microcontroller CPU timing is measured.
  An unchanged frame has zero payload cost; the driver's address-only probe is
  deliberately outside the planner benchmark.

## Recovery policy

A failed data transfer invalidates the shadow and first restores the shared
controller settings and a checked full frame, without display-off or a reset
pulse. Reapplying the settings also covers power loss during the data transfer
that ends before the next address probe. A failed resync escalates. Failed
initialization/configuration/address-window commands or an observed missing
address require reinitialization: a successful framebuffer ACK alone cannot
prove that the controller configuration survived. This distinction intentionally
preserves the earlier startup-failure and power-loss fixes.

Retries after an unsuccessful attempt are at least 1,000 ms apart, including
across uint32 clock wrap. The bus is probed before a reset; an absent device costs
only a probe per retry interval. When a reset pin exists, runtime high/low reset
phases run in `loop()` with minimum 1 ms/10 ms waits rather than `delay()`. They may
last longer with coarse main-loop ticks. Boot reset timing is unchanged. I2C
writes themselves remain synchronous; the change removes the extra blocking
reset delay, not all possible bus latency.

Reinitialization reuses the shared base initialization sequence. Recovery does
not allocate the framebuffer again. A shadow is trusted, and its warning cleared,
only after the frame and any required final on/off command succeed. Rendering
continues while recovery waits so the recovered screen gets the latest image.
