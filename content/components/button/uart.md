---
description: "Instructions for setting up UART buttons in ESPHome that can output arbitrary UART sequences when activated."
title: "UART Button"
params:
  seo:
    description: Instructions for setting up UART buttons in ESPHome that can output arbitrary UART sequences when activated.
    image: uart.svg
---

The `uart` button platform allows you to send a pre-defined sequence of bytes on a
{{< docref "/components/uart" "UART bus" >}} when triggered.

```yaml
# Example configuration entry
button:
  - platform: uart
    name: "UART String Output"
    data: 'DataToSend'
  - platform: uart
    name: "UART Bytes Output"
    data: [0xDE, 0xAD, 0xBE, 0xEF]
```

## Configuration variables

- **data** (**Required**, string or list of bytes): The data to send via UART. Either an ASCII string
  or a list of bytes.

- **uart_id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID of the UART hub.
- All other options from [Button](/components/button#config-button).

## See Also

- {{< docref "/components/uart" >}}
- {{< apiref "uart/button/uart_button.h" "uart/button/uart_button.h" >}}
