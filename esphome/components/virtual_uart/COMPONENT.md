# Virtual UART Component Documentation

## Overview
The Virtual UART component creates a virtual serial interface that can be used with various ESPHome components that normally communicate over UART. It intercepts transmissions from those components into a configurable lambda, and provides an action to inject responses to those components. Can be used for multiple use-cases including testing components without hardware, sending UART messages over non-UART media, or modifying transmissions between a component and a real UART device.

## Features
- **Multiple Virtual UARTs**: Supports multiple virtual UARTs.
- **Stores all UART hardware configurations**: To appear identical to real hardware
- **Provides lambda to intercept TX**: Receive outgoing bytes from UART child components
- **Provides action to inject RX**: Inject a response that the child can then read


## Configuration
To configure the Virtual UART component, add the following to your `yaml` configuration file:

```yaml
virtual_uart:
  id: my_virtual_uart
  on_tx:
    then:
      - lambda: ESP_LOGD( "main", "%s", format_hex_pretty(data).c_str());
```

## Injecting data back to the component (simulated response)

To simulate response UART data can either use the automation, or call inject_rx on the component:
```yaml
on_xxx:
  then:
    - virtual_uart.inject_rx: [0x01, 0x42]
    - lambda: |-
        id(my_virtual_uart).injext_rx({0x01, 0x43});
```
