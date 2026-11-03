# zwave_proxy

## zwave_proxy frame checksums on a live proxy (#20922)

A Z-Wave proxy (ESP32-S3 with the module on `usb_uart`) is a live device: flash it over OTA and read logs
over the API, there is no serial console because the native USB is the host port for the module, so the
boot-time `GET_NETWORK_IDS` exchange is never captured. Home Assistant's Z-Wave JS holds the single API
subscription (it polls `0x3B` every 30 s and resubscribes after every reboot), so do not subscribe from a
script, and keep the device `name:` so it stays on Z-Wave JS; give each build its own scratch directory
instead.

1. `logger: level: VERY_VERBOSE` with `logs: zwave_proxy: VERY_VERBOSE` and DEBUG for the rest: the
   `Sending:`, `Received frame:` and `CHECKSUM Received ... Calculated` lines are `ESP_LOGVV`, which needs
   the compile-time level.
2. `md5` the two `firmware.ota.bin` files to be sure the A and B builds differ, then `esphome upload
   --device <name>.local` and a `timeout 330 esphome logs --device <name>.local` capture started right after
   the OTA to catch the resubscribe traffic.
3. `scripts/zwave_proxy/verify_checksums.py <captures>` recomputes the XOR (init 0xFF, LENGTH..payload) over
   every logged frame and counts bad checksums and ACK/NAK sent and received; `scripts/zwave_proxy/devinfo.py
   <name>.local` prints the firmware version, compile time and the Home ID from the API (a Home ID proves the
   device-built `GET_NETWORK_IDS` frame was accepted at boot).
4. Pre-existing: one `zwave_proxy took a long time for an operation` warning on the first frame after the
   log client connects, caused by VERY_VERBOSE logging; it appears on dev too.
