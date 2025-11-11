---
description: "Instructions for setting up H-Bridge controlled switches (or relays)."
title: "H-bridge Switch"
params:
  seo:
    description: Instructions for setting up H-Bridge controlled switches (or relays).
    image: hbridge-relay.jpg
---

The `hbridge` switch platform allows you to drive an *h-bridge* controlled latching relay.

{{< img src="hbridge-relay.png" alt="Image" caption="Omron G6CK-2117P relay module." width="50.0%" class="align-center" >}}

```yaml
# Example configuration entry
switch:
  - platform: hbridge
    id: my_relay
    name: "Relay"
    on_pin: GPIOXX
    off_pin: GPIOXX
    pulse_length: 50ms
    wait_time: 50ms
```

## Configuration variables

- **on_pin** (**Required**, [Pin Schema](/guides/configuration-types#pin-schema)): The GPIO pin to pulse to turn on the switch.
- **off_pin** (**Required**, [Pin Schema](/guides/configuration-types#pin-schema)): The GPIO pin to pulse to turn off the switch.
- **pulse_length** (*Optional*, [Time](/guides/configuration-types#time)): The length in milliseconds of the pulse sent on `on_pin` and `off_pin` to change switch state. Defaults to `100 ms`.
- **wait_time** (*Optional*, [Time](/guides/configuration-types#time)): The time in milliseconds to delay between pulses on `off_pin` and `on_pin`. Defaults to no delay.
- **optimistic** (*optional*, boolean): Whether to operate in optimistic mode - when in this mode,
  any command sent to the switch will immediately update the reported state. Defaults to `false`, and the reported state updates only at the end of the pulse.

- All other options from [Switch Component](/components/switch#config-switch).

## See Also

- {{< docref "/components/output" >}}
- {{< docref "/components/switch" >}}
- {{< apiref "hbridge/switch/hbridge_switch.h" "hbridge/switch/hbridge_switch.h" >}}
