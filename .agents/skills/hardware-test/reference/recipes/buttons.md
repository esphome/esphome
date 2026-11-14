# buttons

Button platforms converted to `button::ApplyButton` change no behaviour on the wire, so each test compares what a
press does on dev and on the PR: the pad, the action call or the bytes sent. All three run on a D1 mini with no
wiring and no other device.

## output button pad timing (#21907)

`scripts/output_button`: two `gpio` outputs on GPIO5 and GPIO4 and three output buttons (500 ms and 1000 ms on
GPIO5, 200 ms on GPIO4). `edges.hpp` attaches a CHANGE interrupt to each output pad and stamps every edge with
`micros()`; on ESP8266 a pad reads back its own output level. A script runs four cases three times: one press, a second
press 250 ms in (the reset restarts, high 750 ms), both outputs at once, and the 1000 ms button then the 500 ms
button on the same output 300 ms later (low at 800 ms). `run.sh LABEL` flashes, captures 45 s and prints the edge
lines; diff dev against the PR. Each button keeps its own reset timer, so the 1000 ms button's reset fires later on a
pad that is already low and shows no edge.
Results: https://github.com/esphome/esphome/pull/21907#issuecomment-6516597178

## homeassistant button through a raw API client (#21908)

`scripts/homeassistant_button/ha-button-common.yaml` has three `homeassistant` buttons: `button.doorbell`,
`input_button.chime`, and one with no `entity_id` that is an `api: wizard:` input. An `on_boot` press runs before
any client connects. `habdrive.py HOST` is a plaintext API client that subscribes to Home Assistant actions, presses
each button and prints every action call (service and data). It sets the wizard input with `WizardInputSetRequest`
(message 158, key = FNV-1 hash of the input's id), first to an `input_button`, then to a `button`, then to an invalid
value the device must reject. `run.sh LABEL` flashes, captures and drives it. Expect `button.press` or
`input_button.press` per entity, `No entity ID set yet` and no call before the wizard input is set, and `No clients
connected` from the boot press.

- `homeassistant` entities are internal by default, so the client sees no buttons unless they set `internal: false`.
- Wait for each reply type instead of a fixed time: the hello reply can take more than a second, and a fixed wait
  shifts every later reply by one step.
- Results: https://github.com/esphome/esphome/pull/21908#issuecomment-6516693220

## uart button on the UART0 loopback (#21906)

`scripts/uart_button/uart-button-common.yaml` puts four uart buttons on UART0 of a D1 mini: a 10 byte string, five
raw bytes with `0x00` and `0xFF`, the same string again (shares one generated function on the PR), and 64 bytes. It
sets the UART0 loopback bit in `on_boot`, so every press comes back on RX, and `uart: debug: direction: BOTH` logs
both sides. `run.sh LABEL HOST` flashes by USB and runs `ubdrive.py`, which presses each button 3 times over the API
and saves the log. Every `>>>` line must equal the configured payload and its `<<<` line.

- Set the loopback bit at `on_boot` priority -100. At 800 the UART setup runs afterwards and clears it.
- Nothing else reads the bus, so the debugger needs `dummy_receiver: true` to log RX.
- UART0 is the bus, so `logger: baud_rate: 0` and logs go over the API. The device address comes from DHCP or mDNS.
- An RX line sometimes lands after the next press marker when the log stream lags. That is not a byte difference.
- Results: https://github.com/esphome/esphome/pull/21906#issuecomment-6516762337
