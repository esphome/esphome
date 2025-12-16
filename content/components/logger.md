---
description: "Instructions for setting up the central logging component in ESPHome."
title: "Logger Component"
params:
  seo:
    description: Instructions for setting up the central logging component in ESPHome.
    image: file-document-box.svg
---

{{< anchor "logger" >}}

The logger component automatically logs all log messages through the
serial port and through MQTT topics (if there is an MQTT client in the
configuration). By default, all logs with a severity `DEBUG` or higher will be shown.
Increasing the log level severity (to e.g `INFO` or `WARN`  ) can help with the performance of the application and memory size.

> [!NOTE]
> The "severity" of a log message represents the importance of the message, i.e. how critical it is. The severity levels are defined in the [log levels](#logger-log_levels) section.

```yaml
# Example configuration entry
logger:
  level: DEBUG
```

## Configuration variables

- **baud_rate** (*Optional*, int): The baud rate to use for the serial
   UART port. Defaults to `115200`. Set to `0` to disable logging via UART.

- **level** (*Optional*, string): The global log level. Any log message
   with a lower severity will not be shown. Defaults to `DEBUG`.

- **initial_level** (*Optional*, string): The initial log level, which may be varied at run time. Defaults to the same value as `level`.
- **logs** (*Optional*, mapping): Manually set the log level for a
   specific component or tag. See [Manual Log Levels for more information](#logger-manual_tag_specific_levels).

- **runtime_tag_levels** (*Optional*, boolean): Enable runtime per-tag log level changes. This is automatically enabled
   when `logs` is configured or when `logger.set_level` is used with a `tag` parameter. Only needs to be manually
   enabled if calling `set_log_level()` from a lambda or external component. Defaults to `false` (auto-enabled as needed).

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.

Advanced settings:

- **tx_buffer_size** (*Optional*, int): The size of the buffer used
   for log messages. Decrease this if you're having memory problems.
   Defaults to `512`.

- **task_log_buffer_size** (*Optional*, int): **ESP32 only**: The size of the internal thread-safe ring buffer for task log messages.
   This prevents API disconnections when multiple threads attempt to log simultaneously.
   Set to `0` to disable the log buffer. Defaults to `768B`.

- **hardware_uart** (*Optional*, string): The Hardware UART to use for logging. The default varies depending on
   the specific processor/chip and framework you are using. See the [table below](#logger-default_hardware_interfaces).

- **esp8266_store_log_strings_in_flash** (*Optional*, boolean): If set to false, disables storing
   log strings in the flash section of the device (uses more memory). Defaults to true.

- **on_message** (*Optional*, [Automation](/automations)): An action to be
   performed when a message is to be logged. The variables `int level`, `const char* tag` and
   `const char* message` are available for lambda processing.

- **deassert_rts_dtr** (*Optional*, boolean): Causes ESPHome to sequentially drive DTR and RTS false after opening
   a serial logging connection. Defaults to `false`.
   Many ESP boards use these signals to reset the chip or enter
   bootloader mode, and the effect of setting this option will be
   to reset the chip in application mode after opening the serial port, thus ensuring that all log messages
   from the boot process are captured.
   Note: Deassert typically means a TTL high level
   level since RTS/DTR are usually low active signals.

{{< anchor "logger-hardware_uarts" >}}

## Hardware UARTs

The logger component makes use of platform-specific hardware UARTs for serial logging. For example, the ESP32
has three hardware UARTs, all of which can be used for both transmit and receive. The ESP8266 only has two
hardware UARTs, one of which is transmit-only. The ESP8266's `UART0` can also be "swapped" to TX/RX on the
CTS/RTS pins in the event that you need to use GPIO1 and GPIO3 for something else.

Note that many common boards have their USB-to-serial adapters fixed to the default GPIOs used by `UART0`,
so if you use any other configuration you will not get log messages over the on-board USB.

### Default UART GPIO Pins

|          | `UART0`        | `UART0_SWAP`   | `UART1`        | `UART2`        | `USB_CDC` | `USB_SERIAL_JTAG` |
| -------- | -------------- | -------------- | -------------- | -------------- | --------- | ----------------- |
| ESP8266  | TX: 1, RX: 3   | TX: 15, RX: 13 | TX: 2, RX: N/A | N/A            | N/A       | N/A               |
| ESP32    | TX: 1, RX: 3   | N/A            | TX: 10, RX: 9  | TX: 17, RX: 16 | N/A       | N/A               |
| ESP32-C3 | TX: 21, RX: 20 | N/A | Undefined | N/A | N/A | 18/19 |
| ESP32-C5 | TX: 10, RX: 11 | N/A | Undefined | N/A | N/A | 13/14 |
| ESP32-C6 | TX: 16, RX: 17 | N/A | Undefined | N/A | N/A | 12/13 |
| ESP32-C61 | TX: 5, RX: 4 | N/A | Undefined | N/A | N/A | 12/13 |
| ESP32-P4 | TX: 37, RX: 38 | N/A | TX: 10, RX: 11 | N/A | N/A | 24/25 |
| ESP32-S2 | TX: 43, RX: 44 | N/A | TX: 17, RX: 18 | N/A | 19/20 | N/A |
| ESP32-S3 | TX: 43, RX: 44 | N/A | TX: 17, RX: 18 | Undefined | 19/20 | 19/20 |
| NRF52    | pins varies by board | N/A | pins varies by board | Undefined | D+/D- | N/A |

*Undefined* means that the logger component cannot use this hardware UART at this time.

{{< anchor "logger-default_hardware_interfaces" >}}

## Default Hardware Interfaces

Because of the wide variety of boards and processors/chips available, we've selected varying default
hardware interfaces for logging. Many newer boards based on ESP32 variants (such as the C3, S2 and S3)
are using the ESP's on-board USB hardware peripheral while boards based on older processors (such as
the original ESP32 or ESP8266) continue to use USB-to-serial bridge ICs for communication.

|          | Interface |
| -------- | --------- |
| ESP8266  | `UART0`   |
| ESP32    | `UART0`   |
| ESP32-C3 | `USB_SERIAL_JTAG` |
| ESP32-C5 | `USB_SERIAL_JTAG` |
| ESP32-C6 | `USB_SERIAL_JTAG` |
| ESP32-C61 | `USB_SERIAL_JTAG` |
| ESP32-P4 | `USB_SERIAL_JTAG` |
| ESP32-S2 | `USB_CDC`         |
| ESP32-S3 | `USB_SERIAL_JTAG` |
| RP2040   | `USB_CDC` |
| NRF52    | `USB_CDC` |

{{< anchor "logger-log_levels" >}}

## Log Levels

Possible log levels are (sorted by severity):

| Level              | Color  | Description |
| ------------------ | ------ | ----------- |
| `NONE`             |        | No messages are logged. |
| `ERROR`            | Red    | Only errors are logged. Errors prevent the ESP from working correctly. |
| `WARN`             | Yellow | Warnings and errors. Warnings are recoverable issues like invalid sensor readings. |
| `INFO`             | Green  | Errors, warnings and info messages are logged. |
| `DEBUG` (default)  | Cyan   | Everything up to debug. Includes sensor readings and status messages. |
| `VERBOSE`          | Gray   | Like debug, but includes additional messages usually deemed to be spam. |
| `VERY_VERBOSE`     | White  | All internal messages including data flowing through I²C, SPI and UART buses. |

> [!WARNING]
> Using `VERY_VERBOSE` can significantly impact device performance and may cause connection instability.

{{< anchor "logger-manual_tag_specific_levels" >}}

## Manual Tag-Specific Log Levels

If some component is spamming the logs and you want to adjust its log
level, you can set its level in your configuration, by identifiying its tag.

Example: verbose logs globally, but reduce MQTT noise:

```yaml
logger:
  level: VERBOSE
  logs:
    mqtt.component: DEBUG
    mqtt.client: ERROR
```

> [!NOTE]
> When using `logs`, runtime per-tag log level support is automatically enabled. When this feature is disabled
> (the default when `logs` is not configured), the logger is optimized for better performance and reduced memory usage.

The `level` option controls which log statements are included in the
firmware. You cannot set a tag to a more detailed level than
the global one, because log statements with lower severity than that level are not compiled in.
However, you can suppress them using `initial_level`, and enable them for specific tags:

```yaml
logger:
  level: VERBOSE
  initial_level: ERROR
  logs:
    wifi: VERBOSE
```

Here, `VERBOSE` logs are compiled, but not shown (because of `initial_level: ERROR`)
However, the `wifi` tag has `VERBOSE` level enabled, and shown.

{{< anchor "logger-log_action" >}}

## `logger.log` Action

Print a formatted message to the logs.

In the `format` option, you can use `printf`  -style formatting (see [Formatted Text](/components/display#display-printf)).

```yaml
on_...:
  then:
    - logger.log: "Hello World"

    # Formatted:
    - logger.log:
        format: "The temperature sensor reports value %.1f and humidity %.1f"
        args: [ 'id(temperature_sensor).state', 'id(humidity_sensor).state' ]
```

Configuration options:

- **format** (**Required**, string): The format for the message in [printf-style](/components/display#display-printf).
- **args** (*Optional*, list of [lambda](/automations/templates#config-lambda)): The optional arguments for the
   format message.

- **level** (*Optional*, string): The [log level](#logger-log_levels) to print the message
   with. Defaults to `DEBUG`.

- **tag** (*Optional*, string): The tag (seen in front of the message in the logs) to print the message
   with. Defaults to `main`.

## `logger.set_level` Action

Set the log level at runtime. The level can only be set to a level that is no less severe than the global log level.

- **level** (**Required**, string): The new log level to set.
- **tag** (*Optional*, string): The tag to set the log level for. If not set, the global log level will be set.

```yaml
on_...:
  then:
    - logger.set_level: INFO

    - logger.set_level:
        level: DEBUG
        tag: mqtt.client
```

> [!NOTE]
> When using `logger.set_level` with a `tag` parameter, runtime per-tag log level support is automatically enabled.
> If you need to call `set_log_level()` directly from a lambda or external component, you must manually enable
> `runtime_tag_levels: true` in the logger configuration.

## Logger Automation

{{< anchor "logger-on_message" >}}

### `on_message`

This automation will be triggered when a new message is added to the log.
In [lambdas](/automations/templates#config-lambda) you can get the message, log level and tag from the trigger
using `message` (`const char *`  ), `level` (`int`  ) and `tag` (`const char *`  ).

```yaml
logger:
  # ...
  on_message:
    level: ERROR
    then:
      - mqtt.publish:
          topic: some/topic
          payload: !lambda |-
            return "Triggered on_message with level " + to_string(level) + ", tag " + tag + " and message " + message;
```

> [!NOTE]
> Logging will not work in the `on_message` trigger. You can't use the [logger.log](#logger-log_action) action
> and the `ESP_LOGx` logging macros in this automation.

## See Also

- {{< docref "/components/uart" >}}
- {{< docref "/components/select/logger" >}}
- {{< docref "/guides/troubleshooting" >}} - Troubleshooting guide for debugging crashes and boot failures
- {{< apiref "logger/logger.h" "logger/logger.h" >}}
