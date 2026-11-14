# Bluedroid GATT client command queue

## Queued MTU request recursion overflows BTU_TASK (#17954, #22670)

Bluedroid's `bta_gattc_pop_command_to_send()` runs the head of `p_cmd_list` and removes it only after
`bta_gattc_sm_execute()` returns. When that head is an MTU request (or an execute write) whose send fails at once,
the failure path pops again with the same command still at the head, so it recurses until the BTU task stack
overflows. Every ESP-IDF release so far; PR #22670 fixes it in ESPHome with linker wraps
(`bluedroid_gattc_queue.c`).

Rig, `scripts/bluedroid_mtu_recursion/`:

- Peer: an ESP32-S3 with `btusrv.yaml` (`esp32_ble_server`, one readable and writable characteristic, advertising).
- Central: a classic ESP32 devkit with `btu-common.yaml` plus a per-build file like `btucli-example.yaml` (set
  `peer_mac` to the peer's Bluetooth MAC, which is its Wi-Fi MAC + 2, and give each build its own name).
  `CONFIG_FREERTOS_WATCHPOINT_END_OF_STACK: y` makes the overflow panic at the write:
  `Stack canary watchpoint triggered (BTU_TASK)`.
- Each connection: `on_connect` looks up the characteristic handle right away (a `ble_client` with no child nodes
  releases services and cleans the GATT cache in the same SEARCH_CMPL event, so a lookup after a delay finds
  nothing), waits 1.5 s, then suspends `BTU_TASK`, calls `esp_ble_gap_disconnect`, queues 4 reads and one MTU
  request, waits 30 ms and resumes. The batch is the key: sent one at a time, the BTU task handles each request
  before the next arrives and nothing waits in the queue. The reads fail and keep the command slot busy so the MTU
  request is queued; when popped, `GATTC_ConfigureMTU` fails with "BT_GATT: connection not established".
  `ble_client` reconnects on its own and the next round starts.
- `analyze.py CAPTURE` counts rounds, BTU canary crashes, boots, reconnects with and without a reboot and MTU 517.
- Decode with the IDF `xtensa-esp32-elf-addr2line -pfiaC`; the overflow shows the repeating
  `pop_command_to_send (act.c:1739/1746) -> sm_execute -> cfg_mtu (act.c:1042)` frames.

Results on ESP-IDF 5.5.6: dev at 46844d3631 crashed 73 of 73 rounds in 5.5 min (every round, about 4.5 s per
reboot cycle); the PR at ad787163fd ran with no crash and reconnected without a reboot each time. Check the wraps are
linked with `nm firmware.elf | grep __wrap_GATTC_ConfigureMTU` (native ESP-IDF builds only; the define is
`USE_ESP32_BLE_BLUEDROID_GATTC_QUEUE`).

### Execute write variant

`btuxw-example.yaml` sets `exec_write: "true"`, which sends `esp_ble_gattc_execute_write(gattc_if, conn_id, true)`
in place of the MTU request. No prepare write is needed: Bluedroid does not check for prepared writes, and
`GATTC_ExecuteWrite` fails synchronously with the same "connection not established" error. The variant also sets
`CONFIG_BT_LOG_GATT_TRACE_LEVEL_API: y`, so each send logs `BT_GATT: GATTC_ExecuteWrite conn_id=... is_execute=1`,
and those lines are counted per round with awk between the `ROUND n start` lines. In a few rounds (about 3 to 14%)
the controller reports the disconnect before the command is popped. Those rounds log no execute send and never
crash, so count "rounds that reached the send", not all rounds. Most dev backtraces in this variant are cut off in
`vTaskSwitchContext` (the extra trace logging makes task switches more likely at the moment of overflow). Use the
complete ones to decode the `bta_gattc_execute (act.c:1352)` cycle.

Results: dev 46844d3631 had 81 rounds, 70 reached the send, and all 70 crashed. PR e51f0b8321 had 151
rounds, 147 reached the send with exactly one execute line each, and there were 0 crashes and 1 boot.
