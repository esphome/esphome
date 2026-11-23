# RP2 scheduler lock under IRQ and lwIP load (#22688)

On RP2, lwIP callbacks and pico-sdk timers can call into the scheduler from IRQ context while the main loop is inside
it. `components/irq_sched_stress/` reproduces that on purpose: a 300 us pico-sdk repeating timer calls `defer()` and
`set_timeout()` while the main loop does the same. Judge it by counts: every defer runs, and exactly 10 of 400 named
timeouts on 10 shared names run. RP2 is `ESPHOME_THREAD_SINGLE`, so `defer()` order is not FIFO on any build; the
out of order count is informational. Keep `esphome: debug_scheduler:` off, it logs from the IRQ.

- `stress.yaml`: the component, `run_test`, `run_defer` and `run_cancel` API actions, REST entities for the churn
  loops and a 16 ms interval that logs its gaps. `race.yaml` ports the removed item race integration fixture.
  `picow.yaml` and `evb.yaml` add them to the `../rp2_web_ble/` role configs (encrypted API, web_server, diagnostics);
  put `secrets.yaml` here or next to the role configs.
- `flow.sh NAME CONFIG HOST [SERIAL_PORT]`: upload, then the actions, connstress, REST churn and both together; ends
  with PASS or FAIL. `svcrun.py HOST ACTION DONE_REGEX TIMEOUT` runs one action and prints its result lines.
- `restchurn.sh HOST SECS OUT`: 4 parallel REST POST loops. `looptime.sh NAME HOST SECS`: 16 ms interval gaps and
  50 ms re-arms with the API and page loads running, to compare the cost of the lock between builds.

Build both heads with `../pio_build.sh` under different names. Dev before #22688 failed the defer count on the Pico W;
the PR passed every action.
