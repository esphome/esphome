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

- An ESP Thread Border Router board running ESPHome `openthread: border_router: rcp:` works as the border router
  (pins in `boards.md`). Give both it and the device a fixed dataset with `force_dataset: true`; each keeps an old
  stored dataset otherwise. The host learns the route to the OMR /64 from the border router's RA; ping the
  device's OMR address before the first upload.
- Install the smpclient version pinned in requirements. An older one fails with `unexpected keyword argument
  'timeout_s'` and a missing `get_max_cbor_and_data_size`, which looks like a PR bug. Check the version with the
  Python of the venv that runs ESPHome, not a system pip.
- The build holds two `zephyr.uf2` files. Copy `.pioenvs/<name>/zephyr/zephyr.uf2` (the app, from 0x26000 up,
  about 827 KB). `mcuboot/zephyr/zephyr.uf2` (about 70 KB) lands on the app slot and the board stops enumerating;
  double tap RESET and copy the right one.
- To log the device's addresses, format `otIp6GetUnicastAddresses()` by hand: `network::IPAddress::str_to()`
  does not link on nRF52 (no `inet_ntop`).
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
  0x61000 (97 sectors of 4 KB, 0x2f000 to 0x90000) and the app partition 0x60e00 (396,800 B); `mcuboot_secondary`
  is 0x90000 to 0xf1000 and `settings_storage` 0xf2000 to 0xf4000. MCUboot swap using move keeps one sector for
  its trailer and one for the move, so it only swaps in an image up to (97 - 1 - 1) × 4 KB = 389,120 B
  (`app_max_sectors()` and `app_max_size()` in bootutil `swap_move.c`). An image between 389,120 and 396,800 B links,
  uploads and the CLI reports `Successfully uploaded program`, but MCUboot never swaps it in: it stays pending in the
  secondary slot, and every later upload and the SMP image erase fail with `NO_FREE_SLOT`. OTA stays stuck until
  the slot is cleared over USB. Measured: 388,787 B swapped in, 392,155 B did not. `ota.sh` checks `app_update.bin`
  against this limit with `mcuboot_slot.py limit` before it uploads, and afterwards asks the device which image runs
  (`mcuboot_slot.py active`, by the image's SHA-256), since the CLI's exit code alone is not proof. That check passes once the
  new image runs and says whether it has confirmed itself; add `--expect confirmed --wait 300` to prove it stays.
- With #17245 at 03d45d1aaa the build itself fails above the limit (`The application image is N bytes, but MCUboot
  can only swap in L bytes on this board`), and after a confirmed reset the client waits for the device and logs
  `runs the new image` or fails with `rebooted but did not swap in the new image`. After a dropped reset reply it still
  ends with `The reboot of ... is not confirmed` without that wait, so `ota.sh`'s own image check is what proves those
  runs.
- Clearing a stuck pending image: the SMP image erase is refused while the image is pending, and resets do not help.
  Over USB, touch the app's CDC port at 1200 baud to enter the UF2 bootloader, then copy the 16-block UF2 from
  `mcuboot_slot.py blank-uf2 BUILD OUT`, which writes 0xFF over the secondary slot's header page (`mcuboot_secondary`
  in `partitions.yml`, 0x90000 on the ItsyBitsy). The bootloader resets on its own once the last block is written
  (`cp` may report an I/O error then, which is normal), MCUboot finds no image header and clears the slot, and the
  image states show slot 0 only.
- Over USB, the Adafruit UF2 bootloader only accepts blocks inside its app area (its `CURRENT.UF2` maps 0x1000 to
  0xE9000). A UF2 with a block outside that area is never completed, and the bootloader then ignores every
  later file, even a good app UF2, until a physical reset (double tap RESET or replug). Keep hand-made UF2 writes inside the app area.
- Results: https://github.com/esphome/esphome/pull/17245#issuecomment-6516152182,
  https://github.com/esphome/esphome/pull/17245#issuecomment-6518292041 (swap limit),
  https://github.com/esphome/esphome/pull/17245#issuecomment-6519010038 (retest)

## TCP stack only when a component uses TCP (#21910)

`scripts/nrf52_tcp_probe/tcp-probe.yaml` joins the test Thread network with no TCP user and opens a TCP and a UDP
socket twice from an interval. Build it per tree with `-s name NAME`, flash over UF2, capture the USB log, then
`check.sh BUILD CAPTURE`: it reads `CONFIG_NET_TCP` and the packet and buffer counts from
`.pioenvs/<name>/zephyr/zephyr/.config` and wants `tcp null udp ok` twice plus the `TCP stack not built` error exactly
once on a build without TCP, or `tcp ok udp ok` twice and no error with it. Add `api:` to see TCP and the larger pools
come back, and connect with `apicheck.py ADDR` over Thread. Measured on the ItsyBitsy: 12.9 KB less flash and 17.1 KB
less RAM without TCP (Zephyr's default pools, 16/16 instead of 24/48), and SMP over UDP OTA still works with them.
Results: https://github.com/esphome/esphome/pull/21910#issuecomment-6518287139

## sdk-nrf 3.4.1 on the ItsyBitsy (#22233, #22558)

- Set `framework: version: 3.4.1` and `board: adafruit_itsybitsy`.
- Trees before #22233 include `<nrf.h>` in `logger_zephyr.cpp`, which 3.4.1 no longer has on the include path, so
  Adafruit bootloader boards fail to build (`nrf.h: No such file or directory`). To test such a tree on 3.4.1, change
  that include to `<nrfx.h>` locally.
- On the new USB stack, `usb_cdc_acm` bulk echo ran at about half the legacy rate (67 against 121 KiB/s for 64 KiB),
  and `flush()` returned `3`. The logger, DTR, line coding and the 1200 baud touch worked.
- The zephyr_ble_server flow above, on 3.4.1: before #22558 advertising failed to restart after the first disconnect
  (`Advertising failed to start (rc -12)`); with it the server took 39 connections in 120 s.

## ItsyBitsy as a Thread MTD child (#16387)

- Flash by serial DFU (the terminal cannot write to `ITSY840BOOT` on macOS, see SKILL.md). `esphome upload` fails
  about half the time ("Target is not in DFU mode") and can leave the bootloader wedged. Press RESET once, then run
  this on the bootloader port, which worked every time (`scripts/nrf_pwm_probe/i52run.sh` does the same):

  ```sh
  <sdk-nrf python> -m nordicsemi.__main__ dfu serial -pkg <build>/.pioenvs/<name>/firmware.zip \
    -p <bootloader port> -b 115200 --singlebank
  ```
- A 1200 baud touch on the app port takes the child off the air (bootloader); the parent's child table entry goes
  when its age reaches the child's timeout (`recipes/openthread.md`, child checks).

## Zigbee endpoints without a coordinator (#22788)

- Endpoint table on the device: loop `zb_af_get_endpoint_desc(1..240)` and log `simple_desc` (endpoint, profile,
  device ID, cluster list). Attribute values: `zb_zcl_get_attr_desc_a(ep, cluster, ZB_ZCL_CLUSTER_SERVER_ROLE, 0x0055)`.
- Simulated coordinator writes: `zigbee_schedule_callback()` (declare it `extern "C"`) to get on the ZBOSS thread,
  `zb_buf_get_out_delayed(cb)`, fill a `zb_zcl_device_callback_param_t` with `ZB_ZCL_SET_ATTR_VALUE_CB_ID`, endpoint,
  cluster, attr 0x0055 and `values.data8`/`data32`, call `ZCL_CTX().device_cb(bufid)`, log `status`, free the buffer.
  A write to an endpoint with no handler returns -31 (RET_NOT_IMPLEMENTED). `scripts/zigbee_coord/zbep.hpp` has all
  three (`zbdump()`, `inject()`, `readpv()`).
- Pick the dongle's port by USB serial (`ports.py --serial`), never by a port name glob.
- Seen on dev too: a template binary sensor true from boot leaves Binary Input present value 0 (the first state never
  reaches the attribute).

## Zigbee over the air with an ESP32-H2 coordinator (#22788, #23051, #22407, #22482)

Scripts: `scripts/zigbee_coord/`. Results: https://github.com/esphome/esphome/pull/23051#issuecomment-7111856232,
https://github.com/esphome/esphome/pull/22407#issuecomment-7111856772,
https://github.com/esphome/esphome/pull/22482#issuecomment-7111857310

- Coordinator: esp-zigbee-sdk v2.0.5 `examples/all_device_types_app` (console) built for esp32h2 with ESPHome's own IDF
  (`build_coord.sh DIR`, see the ESP Thread BR section of `reference/boards.md` for the IDF environment),
  `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`, the zigbee lib pinned `==2.0.5`, absolute paths for the console component and
  `sdkconfig.mbedtls_minimal`. Patch `cli_cmd_zcl.c` to print `in.header->src_ep` in the read, write and report
  handlers; stock output shows only the local endpoint.
- On the ESP Thread Border Router board an ESPHome OTBR on the S3 resets the H2 every second when the H2 runs anything
  but RCP firmware (its USB port drops about 1 s after each boot). Flash `zbs3hold.yaml` on the S3 first: it pulses
  GPIO7 low then holds it high and leaves GPIO8 alone (driving GPIO8 high puts the H2 in download mode, `boot:0x4`).
  Back up both chips' flash before, to put the border router back afterwards.
- `zbcon.py PORT coord.log coord.fifo SECONDS` bridges the console;
  `zbdrv.py` sends lines, marks the log and parses it (`waitjoin`, `interrogate`, `eps`, `epbydesc`, `parse`; `ZBSRC=0xSHORT`
  filters by source when two devices are on the network). Hexdumps over 8 bytes have a double space after byte 8.
- **Form the network with `network type c` before `bdb_comm start form`.** The console example enables distributed
  security at init, and an ESP32 end device (ESPHome disables distributed security) then fails authentication: the
  coordinator logs only `update: ... status(1)`, never `authorized` or `New device commissioned`, and the device
  reboots every 13 s or so. The reboot is ESPHome's `EZB_ZDO_SIGNAL_LEAVE` (type reset) handler calling
  `esp_zigbee_factory_reset()`; its logs from the Zigbee task are lost in the reset. An nRF joins the distributed
  network fine. `coordsetup.sh` does the full setup.
- Opening the H2 console port resets the H2 (`rst:0x15 USB_UART_HPSYS`), and after any reset `network type` reads
  `distributed` again and the data model is gone: rerun `coordsetup.sh` every time the bridge is restarted.
- The local endpoint needs client clusters before `dm register` (`dm add -e 1 -c 0x000c -r C`, ...), or every
  `zcl send_gen` fails with ESP_ERR_NOT_FOUND; `factoryreset` to change the data model. The console cannot `dm add`
  Level (0x0008) or Color (0x0300) clients (ESP_ERR_INVALID_ARG); `zha add 2 color_dimmer_switch` gives an endpoint
  with On/Off, Level and Color clients. Raw commands: `zcl send_raw -d SHORT --dst-ep EP -e 2 -c 0x0008 --cmd 4 -p
  0x800000` (payload hex bytes in wire order). Float values go in `-v` as wire bytes (42.0 = `0x00002842`,
  `zbdrv.py f32 42`).
- Permit join lasts at most 254 s; `keepopen.sh` reopens it every 200 s. `flashdev.sh` erases the C6 before each build
  so it joins as a new device (the network data lives in the `nvs` partition); the nRF configs use `wipe_on_boot`, and
  a rejoin after it kept the same short address.
- Flows (`COORD_IEEE` from `address -x`, shared steps in `common.sh`): `runi.sh` (entity flow: interrogate, bind and report, writes, 75 s of
  reports; `c6-ents.yaml` or `nrf-ents.yaml` with `CHIP=`), `runnumber.sh` (#23051 number descriptor, writes, restore
  across a reset), `runcopy.sh` (copy number, `c6-copy.yaml`), `runlight.sh` (light On/Off, Level, Color and local change reports),
  `summ.py NAME` (per-build summary for diffing PR against dev).
- Diagnosis tricks that worked: `esp_rom_printf` in the signal handler (synchronous, survives the reset);
  `-Wl,--wrap=nvs_erase_all,--wrap=esp_restart` via `platformio_options: build_flags` plus `wrapdbg.hpp` to print the
  caller; `CONFIG_ZB_DEBUG_MODE: y` in `sdkconfig_options` links the debug lib, which logs `ESP-ZIGBEE` warnings such as
  `[zdo_app_join.c] Comm failed in Auth (0x7)` through the ESPHome logger.
- ESP32 auto endpoints merge different clusters into endpoint 1 (Analog Input, Analog Output, Binary Output, Time),
  unlike the nRF (one endpoint per entity). A `copy` number shows the #23051 trait bug: dev exposes
  Min/Max/Resolution 0/0/0.
