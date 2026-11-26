# usb_cdc_acm

## usb_cdc_acm on the nRF52840 dongle (#11634)

`templates/usb-cdc-acm-dongle-dut.yaml`: logger on USB_CDC port 0, `usb_cdc_acm` interface 0 disabled,
interface 1 echoing from a 20 ms lambda, 0x01-prefixed commands for burst, stream, flush, peek, line coding
and counters. Build and flash with `dfu: true` through the app port; the dongle then shows three CDC ports:
the board DTS node `cdc_acm_uart` (unused), `cdc_acm_uart0` (log) and `cdc_acm_uart1` (data), in that order.
Capture the log port with `serial_capture.py`, drive the data port with `scripts/usb_cdc_acm/drive.py
DATA_PORT` (11 cases, prints PASS/FAIL and a summary) and grep the log for `Opened`, `Closed`, `buffer full`,
`flush result`, `line coding`. Boot lines are held in the driver's 1 KB ring until the host opens the port, so
a capture started later still shows the first 1 KB of the boot and then a truncated line.

With three CDC instances the Zephyr legacy stack's `CONFIG_USB_MAX_NUM_TRANSFERS=4` starves the last instance
(each instance holds an OUT read and an IN ZLP), so interface 1 shows DTR changes but moves no data.
`scripts/usb_cdc_acm/cdc_probe.hpp` (include it, call `cdc_probe()` from an interval) reads the Zephyr driver
directly to tell a dead endpoint from a component bug. Raising the transfer pool to 8, or disabling the
board's `cdc_acm_uart` node, fixes it. Design limit of the driver: `fifo_fill` holds `tx_ready` until the
write callback and `write_array` drops when the ring is full, so a 64 KiB full-duplex echo loses one 64 byte
tail on every buffer size.
