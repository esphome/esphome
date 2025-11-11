---
description: "Instructions for setting up a UART serial bus on ESPs"
title: "UART Bus"
params:
  seo:
    description: Instructions for setting up a UART serial bus on ESPs
    image: uart.svg
---

{{< anchor "uart" >}}

UART is a common serial protocol for many devices. For example, when uploading a binary to your ESP
you have probably used UART to access the chip. UART (or for Arduino often also called Serial) usually
consists of 2 pins:

- **TX**: This line is used to send data to the device at the other end.
- **RX**: This line is used to receive data from the device at the other end.

Please note that the naming of these two pins depends on the chosen perspective and can be ambiguous. For example,
while the ESP might send (`TX`  ) on pin A and receive (`RX`  ) data on pin B, from the other device's
perspective these two pins are switched (i.e. *it* sends on pin B and receives on pin A). So you might
need to try with the two pins switched if it doesn't work immediately.

Additionally, each UART bus can operate at different speeds (baud rates), so ESPHome needs to know what speed to
receive/send data at using the `baud_rate` option. Two common baud rates are 9600 and 115200.

In some cases only **TX** or **RX** exists as the device at the other end only accepts data or sends data.

The UART component may be used as a platform for the [Packet Transport Component](/components/packet_transport#packet-transport) component, enabling
sensor data to be sent directly from one ESPHome node to another over a UART bus. When using RS485 this can operate in
a multi-drop configuration.

> [!NOTE]
> On the ESP32, this component uses the hardware UART units and is thus very accurate. On the ESP8266 however,
> ESPHome has to use a software implementation as there are no other hardware UART units available other than the
> ones used for logging. Therefore the UART data on the ESP8266 can have occasional data glitches especially with
> higher baud rates.

> [!NOTE]
> From ESPHome 2021.8 the `ESP8266SoftwareSerial` UART `write_byte` function had the parity bit fixed to be correct
> for the data being sent. This could cause unexpected issues if you are using the Software UART and have devices that
> explicity check the parity. Most likely you will need to flip the `parity` flag in YAML.

> [!NOTE]
> UART implementation for the host platform does not use TX and RX pins but port names.

```yaml
# Example configuration entry
uart:
  tx_pin: GPIOXX
  rx_pin: GPIOXX
  baud_rate: 9600
```

## Configuration variables

- **baud_rate** (**Required**, int): The baud rate of the UART bus.
- **tx_pin** (*Optional*, [Pin](/guides/configuration-types#pin)): The pin to send data to from the ESP's perspective. Use the full pin
  schema and set `inverted: true` to invert logic levels. Not supported by host platform.

- **rx_pin** (*Optional*, [Pin](/guides/configuration-types#pin)): The pin to receive data on from the ESP's perspective. Use the full pin
  schema and set `inverted: true` to invert logic levels. Not supported by host platform.

- **flow_control_pin** (*Optional*, [Pin](/guides/configuration-types#pin)): ESP32 only. The pin used to for hardware RS485 flow control.
  Use of this setting enables half-duplex mode. Use the full pin schema and set `inverted: true` to invert logic levels.

- **port** (*Optional*, string): Host platform only. Unix style name of the port to use.
- **rx_buffer_size** (*Optional*, int): The size of the buffer used for receiving UART messages. Increase if you use an
  integration that needs to read big payloads from UART. Defaults to `256`.

- **rx_full_threshold** (*Optional*, int): ESP32 only. After receiving this number of bytes, the data becomes available for processing.
  The default is calculated at compilation time to be approximately ten milliseconds (about 8 bytes at 9600 baud, 114 bytes at 115200 baud).
  
- **rx_timeout** (*Optional*, int): ESP32 only. This value specifies a number of bytes used to determine the duration of the timeout. The duration of the timeout is based on the amount of time it would take to receive this number of bytes. In other words, for a given value and baud rate, doubling the baud rate will halve the duration of the timeout. After the timeout has elapsed, the data becomes available for processing. Defaults to `2`.
- **data_bits** (*Optional*, int): The number of data bits used on the UART bus. Options: 5 to 8. Defaults to 8.
- **parity** (*Optional*): The parity used on the UART bus. Options: `NONE`, `EVEN`, `ODD`. Defaults to `NONE`.
- **stop_bits** (*Optional*, int): The number of stop bits to send. Options: 1, 2. Defaults to 1.
- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID for this UART hub if you need multiple UART hubs.
- **debug** (*Optional*, mapping): Options for debugging communication on the UART hub, see [Debugging](#uart-debugging).

{{< anchor "uart-hardware_uarts" >}}

## Hardware UARTs

Whenever possible, ESPHome will use the hardware UART unit on the ESP8266 for fast and accurate communication.
When the hardware UARTs are all occupied, ESPHome will fall back to a software implementation that may not
be accurate at higher baud rates.

`UART0` is (by default) used by the {{< docref "/components/logger" "logger component" >}}, using `tx_pin: GPIO1` and
`rx_pin: GPIO3`. If you configure a UART that overlaps with these pins, you can share the hardware with the
logger and leave others available. If you have configured the logger to use a different hardware UART, the pins
used for hardware sharing change accordingly.

The original ESP32 has three UARTs. ESP32 variants (ESP32-C3, ESP32-S2, etc.) may have a different number of UARTs
(usually between two to six). Any pair of GPIO pins can be used, as long as they support the proper output/input modes.

The ESP8266 has two UARTs; the second of which is TX-only. Only a limited set of pins can be used. `UART0` may
use either `tx_pin: GPIO1` and `rx_pin: GPIO3`, or `tx_pin: GPIO15` and `rx_pin: GPIO13`. `UART1` must
use `tx_pin: GPIO2`. Any other combination of pins will result in use of a software UART.

> [!NOTE]
> The Software UART is only available on the ESP8266. It is not available on ESP32 and variants.

{{< anchor "uart-write_action" >}}

## `uart.write` Action

This [Action](/automations/actions#all-actions) sends a defined UART signal to the given UART bus.

```yaml
on_...:
  - uart.write: 'Hello World'

  # For escape characters, you must use double quotes!
  - uart.write: "Hello World\r\n"

  # Raw data
  - uart.write: [0x00, 0x20, 0x42]

  # Templated, return type is std::vector<uint8_t>
  - uart.write: !lambda
      return {0x00, 0x20, 0x42};

  # in case you need to specify the uart id
  - uart.write:
      id: my_second_uart
      data: 'other data'
```

{{< anchor "uart-debugging" >}}

## Debugging

If you need insight in the communication that is being sent and/or received over a UART bus, then you can make use
of the debugging feature.

```yaml
# Example configuration entry
uart:
  baud_rate: 115200
  debug:
    direction: BOTH
    dummy_receiver: false
    after:
      delimiter: "\n"
    sequence:
      - lambda: UARTDebug::log_string(direction, bytes);

# Minimal configuration example, logs hex strings by default
uart:
  baud_rate: 9600
  debug:
```

### Configuration variables

- **direction** (*Optional*, enum): The direction of communication to debug, one of: "RX" (receive, incoming),
  "TX" (send, outgoing) or "BOTH". Defaults to "BOTH".

- **dummy_receiver** (*Optional*, boolean): Whether or not to enable the dummy receiver feature. The debugger
  will only accumulate bytes that are actually read or sent by a UART device component. This feature is
  useful when you want to debug all incoming communication, while no UART device component is configured
  for the UART bus (yet). This is especially useful for developers. Normally you'd want to leave this
  option disabled. Defaults to false.

- **after** (*Optional*, mapping): The debugger accumulates bytes of communication. This option defines when
  to trigger publishing the accumulated bytes. The possible options are:

  - **bytes** (*Optional*, int): Trigger after accumulating the specified number of bytes. Defaults to 150.
  - **timeout** (*Optional*, [Time](/guides/configuration-types#time)): Trigger after no communication has been seen during the
    specified timeout, while one or more bytes have been accumulated. Defaults to 100ms.

  - **delimiter** (*Optional*, string or list of bytes): Trigger after the specified sequence of bytes is
    detected in the communication.

- **sequence** (*Optional*, [Action](/automations/actions#all-actions)): Action(s) to perform for publishing debugging data.
  Defaults to an action that logs the bytes in hex format. The actions can make use of the following variables:

  - **direction**: `uart::UART_DIRECTION_RX` or `uart::UART_DIRECTION_TX`
  - **bytes**: `std::vector<uint8_t>` containing the accumulated bytes

  **Helper functions for logging**

  Helper functions are provided to make logging of debug data in various formats easy:

  - **UARTDebug::log_hex(direction, bytes, char separator)** Log the bytes as hex values, separated by the provided
    separator character.

  - **UARTDebug::log_string(direction, bytes)** Log the bytes as string values, escaping unprintable characters.
  - **UARTDebug::log_int(direction, bytes, char separator)** Log the bytes as integer values, separated by the provided
    separator character.

  - **UARTDebug::log_binary(direction, bytes, char separator)** Log the bytes as `<binary> (<hex>)` values,
    separated by the provided separator character.

  **Logger buffer size**

  Beware that the `logger` component uses a limited buffer size of 512 bytes by default. If the UART
  debugger log lines become too long, then you will notice that they end up truncated in the log output.

  In that case, either make sure that the debugger outputs less data per log line (e.g. by setting the
  `after.bytes` option to a lower value) or increase the logger buffer size using the logger
  `tx_buffer_size` option.

{{< anchor "uart-runtime_change" >}}

## Changing at runtime

There are scenarios where you might need to adjust UART parameters during runtime to enhance communication efficiency
and adapt to varying operational conditions. ESPHome facilitates this through lambda calls.
Below are the methods to read current settings and modify them dynamically:

- **Reading Current Settings:** Access UART's current configuration using these read-only attributes:

```cpp
    // RX buffer size
    id(my_uart).get_rx_buffer_size();
    // RX FIFO full threshold
    id(my_uart).get_rx_full_threshold();
    // RX timeout
    id(my_uart).get_rx_timeout();
    // Stop bits
    id(my_uart).get_stop_bits();
    // Data bits
    id(my_uart).get_data_bits();
    // Parity
    id(my_uart).get_parity();
    // Baud rate
    id(my_uart).get_baud_rate();
```

- **Modifying Settings at Runtime:** You can change certain UART parameters during runtime.
  After setting new values, invoke `load_settings()` (ESP only) to apply these changes:

```yaml
    select:
      - id: change_baud_rate
        name: Baud rate
        platform: template
        options:
          - "2400"
          - "9600"
          - "38400"
          - "57600"
          - "115200"
          - "256000"
          - "512000"
          - "921600"
        initial_option: "115200"
        optimistic: true
        restore_value: True
        internal: false
        entity_category: config
        icon: mdi:swap-horizontal
        set_action:
          - lambda: |-
              id(my_uart).flush();
              uint32_t new_baud_rate = stoi(x);
              ESP_LOGD("change_baud_rate", "Changing baud rate from %i to %i",id(my_uart).get_baud_rate(), 
                       new_baud_rate);
              if (id(my_uart).get_baud_rate() != new_baud_rate) {
                id(my_uart).set_baud_rate(new_baud_rate);
                id(my_uart).load_settings();
              }
```

  Available methods for runtime changes:

```cpp
    // Set TX/RX/Flow control pins
    id(my_uart).set_tx_pin(InternalGPIOPin *tx_pin);
    id(my_uart).set_rx_pin(InternalGPIOPin *rx_pin);
    id(my_uart).set_flow_control_pin(InternalGPIOPin *flow_control_pin);
    // RX buffer size
    id(my_uart).set_rx_buffer_size(size_t rx_buffer_size);
    // RX FIFO full threshold (in bytes units)
    id(my_uart).set_rx_full_threshold(size_t rx_full_threshold);
    // RX FIFO full threshold (in time units)
    id(my_uart).set_rx_full_threshold_ms(uint8_t time);
    // RX timeout
    id(my_uart).set_rx_timeout(size_t rx_timeout);
    // Stop bits
    id(my_uart).set_stop_bits(uint8_t stop_bits);
    // Data bits
    id(my_uart).set_data_bits(uint8_t data_bits);
    // Parity
    id(my_uart).set_parity(UARTParityOptions parity);
    // Baud rate
    id(my_uart).set_baud_rate(uint32_t baud_rate);
```

This flexibility allows for dynamic adaptation to different communication requirements, enhancing the versatility of
your ESPHome setup.

## UART component with the host platform

Since the host platform does not have physical UART pins, the UART component is implemented using Unix-style ports.
Instead of using pins, you can specify the port name to use. This implementation also supports components that have
`require_tx` and `require_rx` options such as smt100 etc.

```yaml
# Example configuration entry for host platform
uart:
  baud_rate: 9600
  port: "/dev/ttyUSB0"
```

## See Also

- {{< docref "/components/logger" >}}
- {{< docref "/components/packet_transport/uart" >}}
- {{< apiref "uart/uart.h" "uart/uart.h" >}}
