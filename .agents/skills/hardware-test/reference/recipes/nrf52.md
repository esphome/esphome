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

## SMP over UDP OTA over Thread, with reset faults (#17245, #21910)

Setup: a Thread border router (see openthread.md) whose OMR prefix the laptop routes to, and an ItsyBitsy nRF52840
(the Adafruit bootloader carries the MCUboot layout; the nRF52840 Dongle's Open DFU bootloader is rejected for
`zephyr_mcumgr` OTA). `scripts/smp_udp_ota/smp-udp-ota-dut.yaml` joins the network with `force_dataset: true` and
logs its build label every 10 s. The first image goes in over UF2, every later one with
`ota.sh MODE YAML VER ADDR` to the device's OMR address. A full image (about 380 KB) takes about 130 s.

- Install the smpclient version pinned in requirements. An older one fails with `unexpected keyword argument
  'timeout_s'`, which looks like a PR bug.
- A new image refuses the next upload (`No free slot. Testing mode but not confirmed yet.`) until it confirms itself.
  Wait about 4 minutes between uploads.
- An image that is already in the secondary slot is only marked and reset (a few seconds). Change `ver` for every
  upload that has to transfer, and expect a host-only change to rebuild byte-identical firmware.
- `inject.py` faults the reset on the host: `lostreply` drops the reply of a reset the device got, `lostreq` drops the
  first request, `lostreq2` drops every reset. The device resets after a short delay, so even after a lost reply the
  client usually still finds the old image running with the new one pending and resends the reset to the old image,
  which is safe. `lostreq2` must end in an error with the old image still running. `ota.sh` ends with `check.sh`,
  which prints PASS or FAIL for the mode, and `inject.py` fails the run when no reset was faulted.
- Without `libc_nano` the config overflows the 396,800 B slot by about 4 KB with Zephyr's TCP stack linked (about
  11 KB of it, unused by SMP over UDP). With TCP left out (#21910) it fits with the full libc. A config with `api:`
  keeps TCP.
- **The real OTA size limit is lower than the link limit.** The nRF Connect SDK partition manager overrides the device
  tree, so read `partitions.yml` in the build directory, not `zephyr.dts`. On the ItsyBitsy layout `mcuboot_primary` is
  0x61000 (97 sectors of 4 KB) and the app partition 0x60e00 (396,800 B). MCUboot swap using move keeps one sector for
  its trailer and one for the move, so it only swaps in an image up to (97 - 1 - 1) × 4 KB = 389,120 B
  (`app_max_sectors()` and `app_max_size()` in bootutil `swap_move.c`). An image between 389,120 and 396,800 B links,
  uploads and the CLI reports `Successfully uploaded program`, but MCUboot never swaps it in: it stays pending in the
  secondary slot, and every later upload and the SMP image erase fail with `NO_FREE_SLOT`. OTA stays stuck until
  the slot is cleared over USB. Measured: 388,787 B swapped in, 392,155 B did not. Check `app_update.bin` against
  this limit before an OTA test.
- Over USB, the Adafruit UF2 bootloader only accepts blocks inside its app area (its `CURRENT.UF2` maps 0x1000 to
  0xE9000). A UF2 with a block outside that area is never completed, and the bootloader then waits until a physical
  reset, ignoring later files. Keep hand-made UF2 writes inside the app area.
- Results: https://github.com/esphome/esphome/pull/17245#issuecomment-6516152182
