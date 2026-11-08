# scheduler

## Scheduler and loop wake A/B (#20356)

An external component whose template button arms an SDK `os_timer` (50 ms, one shot) on the ESP8266;
the callback runs in SYS context, records `millis()` and calls `set_timeout` or `defer`; the timeout
logs the elapsed time and loop count delta. Drive with an aioesphomeapi script (`subscribe_logs` plus
`button_command`). Build the same yaml twice with different names, one per tree.

## interval startup_delay and control from lambdas (#21799)

`scripts/interval/interval-common.yaml` has plain 1 s and 2.5 s intervals, a 1 s interval with `startup_delay: 5s`, a
3 s one with `startup_delay: 500ms`, and a 15 s control interval (`startup_delay: 0s`). The control interval
changes the plain 1 s interval to 500 ms with `id(i_plain).set_update_interval(500)` and `start_poller()`, then
stops it and the 5 s delayed one with `stop_poller()`. Every run logs `millis()`. `run.sh LABEL` flashes a D1 mini,
captures 40 s from boot and runs `ivsum.py`, which prints the run count, first run after boot and gaps per interval.

- Check that each delayed interval's first run comes at or after its delay, that the gaps equal the interval, that
  the 500 ms change takes effect, and that nothing runs after a stop.
- The first run of an interval without a delay falls at a different time on every boot (the scheduler spreads first
  runs), so do not compare those offsets between builds.
- `main.cpp` shows the class each interval gets (`IntervalTrigger` or `DelayedIntervalTrigger` on #21799).
- Results: https://github.com/esphome/esphome/pull/21799#issuecomment-6516798398
