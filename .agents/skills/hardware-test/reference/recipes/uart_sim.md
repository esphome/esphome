# Simulated UART sensors on a classic ESP32 (#21449, #21446)

No sensor needed. `templates/uart_sim/sim.hpp` (included with `esphome: includes:`) has frame builders for wts01,
ds1603l, sm300d2 and ufm01, `sim::loopback(id(uart))` (ESP-IDF `uart_set_loop_back` on the component's own UART),
`sim::send()` that logs the bytes, and `sim::probe`, a SEN0465 responder polled from a 10 ms interval. Each run file
sets `esphome: name:` and includes `fr-common.yaml` plus one of the `fr*-parts.yaml` files as packages.

- Push-style sensors: put the component on a UART, switch it to loopback in `on_boot`, and inject frames from
  template buttons (`fra-parts.yaml`, `frb-parts.yaml`). ufm01 enters its active stream by itself when a 1 s
  interval streams frames during its 2 s startup wait.
- Request/response sensors: cross-wire two UARTs through the GPIO matrix (UART1 tx 25 rx 26, UART2 tx 26 rx 25,
  `allow_other_uses: true` on all four pins); the second UART runs the simulated device (`frc-parts.yaml`).
  A select sets the probe's misbehaviour (tail, FF tail, silent, split, wrong gas).
- Polled readers: run `update()` from a script and log `micros()` around it to measure blocking
  (sm300d2 on dev blocks about 104 ms on an empty buffer).
- The logger stays on UART0, so three builds cover five components. Drive with `scripts/uart_sim/frdrive.py HOST
  LOG press:NAME sel:NAME=OPT num:NAME=V wait:S`, or `scripts/uart_sim/run.sh NAME TREE SECONDS OPS...` (flash,
  capture, drive; `PORT` and `SC` in the environment).
