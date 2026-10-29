# nrf52

## nRF52840 dongle boards (#19009, #20814)

Test config: the two LEDs as inverted GPIO outputs, the button as an inverted pull-up binary sensor, a
10 s alive interval. To retest the nrfutil download path, move the cached `sdk-nrf/nrfutil` aside first.

## ItsyBitsy over UF2 when serial DFU fails (#20481)

Serial DFU to the ItsyBitsy nRF52840 fails about half the time ("Target is not in DFU mode"). Reliable path: a
1200 baud touch on the app port, wait for the `ITSY840BOOT` volume, convert `zephyr/zephyr.hex` with the SDK's
`zephyr/scripts/build/uf2conv.py -f 0xADA52840 -c` and copy the .uf2 to the volume (the copy ends in an I/O error
when the board reboots, which is normal). macOS BLE scans give UUIDs, not MAC addresses.

## zephyr_ble_server connection slot under reordered callbacks (#21723)

`scripts/zephyr_ble_server`: the ItsyBitsy runs `server.yaml` (one connection slot) and an ESP32 runs `client.yaml`,
which connects, drops the link 300 ms later and reconnects as soon as the server advertises again. `run.sh NAME 90`
flashes the UF2, counts the server's `Connected`/`Disconnected`/`Advertising` lines and the client's connects,
and ends with PASS (more than one server connection) or FAIL; an empty capture or a silent client fails the setup.

- The connect and disconnect callbacks each `defer()` to the main loop and the bug needs the disconnect handled
  first, which cannot be forced over the air. Force it with a test-only patch on both trees: replace the connect
  callback's `defer(...)` with `set_timeout(5000, ...)`. A leaked reference then shows as one connection and nothing
  after it (the dump still lists the closed link); a fixed build keeps cycling (17 in 90 s on #21723).
- `dfu: true` makes the next flash a 1200 baud touch; an app built without it needs a double tap on RESET. The
  Zephyr app enumerates as `2fe3:0100`, the bootloader as `239a:0051`, so find the board by its USB serial number.
- macOS is a poor central here: it keeps old bonds with a board (`Peer removed pairing information` on connect)
  and clearing them is a system setting. Use an ESP32 `ble_client`; the server's random static address comes from
  its own log.
