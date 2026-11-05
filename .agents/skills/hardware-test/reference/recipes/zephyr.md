# zephyr

## Zephyr preferences on the nRF52840 dongle (#20810)

`templates/nrf52-preferences-dut.yaml`: logger on USB_CDC at VERBOSE, `flash_write_interval: 5s`, one
`globals` with `restore_value` incremented every 15 s. Give each build its own global id so each first boot
sees a never-saved key (settings survive a reflash). Flow per build: flash through the log port, 40 s capture
of the first boot, then start a second capture and reboot with a 2001 baud touch on the dongle's other CDC
port (the dfu rate callback is global, and the capture holds the log port); `serial_capture.py` reopens the
log port after the USB re-enumeration and gets the buffered boot lines (`load setting`, `Loaded N settings`).
