# esp32_ble_server

## Stack retest: YAML server, Improv, runtime services (#22457, #22458, #22461)

`scripts/ble_server_stack/`: `bles2.yaml` is an ESP32-S3 GATT server with read, notify, write (`set_value` plus
`notify` from `on_write`) and descriptor write characteristics, Improv over BLE and runtime service control from
`rt.hpp` (write 1 create plus queued start, 2 stop, 3 start, 4 remove of a 0x181A service to 5e5e00c0; it runs from an
interval, or inside `on_write` with bit 7 set). `bles2b.yaml` adds a caller-owned Battery service through
`BLEServer::add_service()` (PR builds only), `bles2c.yaml` one made with `create_service()` at boot (dev too).
`flow.sh NAME YAML meas|test|grow` (env TREE, PORT, PY, PROXY, API_KEY, SECRETS): `meas` erases, flashes and logs 60 s
of idle heap; `test` erases, flashes, provisions Wi-Fi with `improv2.py` (a bad checksum first, then the real
credentials), runs 3 `gattc.py` rounds (bleak: table, reads, notify, writes, Device Information, Battery), 3 hard
resets with a round each, the proxy view with `px.py PROXY NAME table` and `rt 0,1,2,3,4`, the same runtime steps
directly with `rtmac.py`, and finally one create inside `on_write` after a fresh boot (`grow.py NAME 1 0x81`), then
counts the device log lines.

- Creating a service from inside an `on_write` lambda crashes dev (LoadProhibited in the GATT event loop; the services
  list grows while it is walked). Run runtime service changes from an interval instead.
- The default `CONFIG_BT_GATT_MAX_SR_PROFILES=8` silently caps the service count; raise it in `sdkconfig_options`.
- Runtime services that share a characteristic UUID: reads time out (ADD_CHAR_EVT is matched by UUID only); give each
  its own.
- The proxy side needs the raw advertisement subscription kept open (see `bluetooth_proxy.md`); `px.py` does.

## Create inside on_write (#22877)

Command 5 in `rt.hpp` creates service 0x1820 + n (one 0x2B10 + n characteristic reading `n 77`) and logs the services
list size, capacity and data pointer (`MOVED` when it reallocated) through the explicit instantiation access trick.
Write 0x85 to create inside `on_write`, 5 to create from the interval. `flow.sh NAME YAML grow` (`REST=1` skips to the
growth) does the `test` start, then `grow.py NAME 8` (eight creates, one write per connection, then the grown services
listed), the proxy table and a round after the growth; `list.py NAME` prints the table directly.

- dev crashes only on a create that reallocates the list, so grow until the capacity changes before calling it fixed.
- bleak on macOS: after the GATT table changes, a second write on the same link never goes out (bleak hangs); do one
  write per connection with `asyncio.wait_for`.
- The ESP32 crash report survives esptool hard resets and reflashes; a later build prints it as "captured by a
  different build", so clear it or note the build before reading one.
