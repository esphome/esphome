---
description: "Instructions for setting up UART event in ESPHome that can be triggered based on incoming UART data."
title: "UART Event"
params:
  seo:
    description: Instructions for setting up UART event in ESPHome that can be triggered based on incoming UART data.
    image: uart.svg
---

The `uart` event platform monitors incoming data on the [UART bus](/components/uart)
and triggers events when predefined byte sequences are detected. Patterns are matched
against the end of received data, making them ideal for detecting message terminators
or commands with known endings.

```yaml
# Example configuration entry
event:
  - platform: uart
    name: "UART Event"
    event_types:
      - "string_event_A": "*A#"
      - "bytes_event_B": [0x2A, 0x42, 0x23]
```

## Configuration variables

- **event_types** (**Required**, list): A list of custom event identifiers that this UART event is capable of triggering,
  where each event identifier is defined by an ASCII string or a list of bytes.
  These identifiers can be used in Home Assistant automations or ESPHome scripts to perform actions when the event occurs.  
  Note: Avoid patterns where one is a prefix of another (e.g., "AB" and "ABC").
  The shorter pattern will match first, preventing the longer one from ever triggering.
  The ideal definition is to have fixed prefix and suffix characters as clear data boundaries.
- **uart_id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID of the UART hub.
- All other options from [Event](/components/event#config-event).

## See Also

- {{< docref "/components/uart" >}}
- {{< apiref "uart/event/uart_event.h" "uart/event/uart_event.h" >}}
