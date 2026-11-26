# UART TX break

## TX break level checks (#22986)

`scripts/uart_tx_break/ext/` has two bench components for a tree with `uart.request_tx_break()` and
`UARTComponent::set_tx_break()`.

`txbrk` (ESP32 and ESP8266; uart_id, tx_gpio, rx_gpio, loopback, live) runs one hold and release cycle per
`run_next(inverted)` call, rotating a plain release, `load_settings()` and `apply_settings_live()` with a baud switch
during the hold, and prints one `CYC` line per cycle and `DONE n/21`. `txbrk-s3.yaml` needs a jumper from GPIO17 to
GPIO18; `txbrk-esp8266.yaml` needs no wiring.

- ESP32-S3: the jumper gives a real wire loopback; `gpio_get_level()` after `gpio_input_enable()` reads both pads,
  and `UART_LL_GET_HW(n)->conf0.txd_inv` shows the inversion bit.
- ESP8266: `hw_uart1: true` on the GPIO2 instance reads the UART1 TX level through its own receiver, since the
  GPIO input register shows 0 there (boards.md, ESP8266).

`brkbench` (LibreTiny) is described in its `__init__.py`; `brk-bw15.yaml` (wire PA03 to PA02) and `brk-bk7238.yaml`
(jumper P0 to P1) use it, `-s baud 115200` for the second speed. It reads the RX pin with a pull opposite to the
expected level, so a floating pin fails, and drives TX high once per break as a positive control.

- RTL8720C (BW15): the TX pin stays claimed by the GPIO driver after the break, so the port cannot take it back
  (boards.md, BW15); `fix: true` frees it with `pinModeRemove()` after each release.
- BK7238: drain the boot text that loops back through the jumper before the first check (boards.md, BK7238).
