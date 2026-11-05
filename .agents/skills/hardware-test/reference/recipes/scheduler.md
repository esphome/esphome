# scheduler

## Scheduler and loop wake A/B (#20356)

An external component whose template button arms an SDK `os_timer` (50 ms, one shot) on the ESP8266;
the callback runs in SYS context, records `millis()` and calls `set_timeout` or `defer`; the timeout
logs the elapsed time and loop count delta. Drive with an aioesphomeapi script (`subscribe_logs` plus
`button_command`). Build the same yaml twice with different names, one per tree.
