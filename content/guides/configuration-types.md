---
description: "Documentation of different configuration types in ESPHome"
title: "Configuration Types"
params:
  seo:
    description: Documentation of different configuration types in ESPHome
    image: settings.svg
---

ESPHome's configuration files have several configuration types. This
page describes them.

{{< anchor "config-id" >}}

## ID

Quite an important aspect of ESPHome are “IDs”. They are used to
connect components from different domains. For example, you define an
output component together with an ID and then later specify that same ID
in the light component. IDs should always be unique within a
configuration and ESPHome will warn you if you try to use the same
ID twice.

Because ESPHome converts your configuration into C++ code and the
IDs are in reality just C++ variable names, they must also adhere to
[C++'s naming conventions](https://en.cppreference.com/w/cpp/language/identifiers)…

- … must start with a letter and can end with numbers.
- … must not have a space in the name.
- … can not have special characters except the underscore (“_“).
- … must not be a keyword.

> [!NOTE]
> These IDs are used only within ESPHome and are not translated to Home Assistant's Entity ID.

{{< anchor "config-pin" >}}

## Pin

ESPHome always uses the **chip-internal GPIO numbers**. These
internal numbers are always integers like `16` and can be prefixed by
`GPIO`. For example to use the pin with the **internal** GPIO number 16,
you could type `GPIO16` or just `16`.

Most boards however have aliases for certain pins. For example the NodeMCU
ESP8266 uses pin names `D0` through `D8` as aliases for the internal GPIO
pin numbers. Each board (defined in {{< docref "/components/esphome" "ESPHome section" >}})
has their own aliases and so not all of them are supported yet. For example,
for the `D0` (as printed on the PCB silkscreen) pin on the NodeMCU ESP8266
has the internal GPIO name `GPIO16`, but also has an alias `D0`. So using
either one of these names in your configuration will lead to the same result.

```yaml
some_config_option:
  pin: GPIO16

some_config_option:
  # alias on the NodeMCU ESP8266:
  pin: D0
```

{{< anchor "config-pin_schema" >}}

## Pin Schema

In some places, ESPHome also supports a more advanced “pin schema”.

```yaml
some_config_option:
  # Basic:
  pin: GPIOXX

  # Advanced:
  pin:
    number: GPIOXX
    inverted: true
    mode:
      input: true
      pullup: true
```

Configuration variables:

- **number** (**Required**, pin): The pin number.
- **inverted** (*Optional*, boolean): If all read and written values
   should be treated as inverted. Defaults to `false`.

- **allow_other_uses** (*Optional*, boolean): If the pin is also specified elsewhere in the configuration.
   By default multiple uses of the same pin will be flagged as an error. This option will suppress the error and is
   intended for rare cases where a pin is shared between multiple components. Defaults to `false`.

- **mode** (*Optional*, string or mapping): Configures the pin to behave in different
   modes like input or output. The default value depends on the context.
   Accepts either a shorthand string or a mapping where each feature can be individually
   enabled/disabled:

  - **input** (*Optional*, boolean): If true, configure the pin as an input.
  - **output** (*Optional*, boolean): If true, configure the pin as an output.
  - **pullup** (*Optional*, boolean): Activate internal pullup resistors on the pin.
  - **pulldown** (*Optional*, boolean): Activate internal pulldown resistors on the pin.
  - **open_drain** (*Optional*, boolean): Set the pin to open-drain (as opposed to push-pull).
     The active pin state will then result in a high-impedance state.

   For compatibility some shorthand modes can also be used.

  - `INPUT`
  - `OUTPUT`
  - `OUTPUT_OPEN_DRAIN`
  - `ANALOG`
  - `INPUT_PULLUP`
  - `INPUT_PULLDOWN`
  - `INPUT_OUTPUT_OPEN_DRAIN`

Advanced options:

- **drive_strength** (*Optional*, string): On ESP32s with esp-idf framework the pad drive strength,
  i.e. the maximum amount of current can additionally be set. Defaults to `20mA`.
  Options are `5mA`, `10mA`, `20mA`, `40mA`.

- **ignore_strapping_warning** (*Optional*, boolean): Certain pins on ESP32s are designated *strapping pins* and are read
  by the chip on reset to configure initial operation, e.g. to enable bootstrap mode.
  Using such pins for I/O should be avoided and ESPHome will warn if I/O is configured on a strapping pin.

  For more detail see [Why am I getting a warning about strapping pins?](/guides/faq#strapping-warnings).

  If you are *absolutely* sure that you are using a strapping pin for I/O in a way that will not cause problems,
  you can suppress the warning by setting this option to `true` in the pin configuration.

- **ignore_pin_validation_error** (*Optional*, boolean): Certain pins on ESP32 chips are reserved
  for internal functions like flash memory interface (for example, GPIO 6-11 on most ESP32 variants). ESPHome
  will raise an error if you try to use these reserved pins.

  However, some ESP32 board designs wire specific flash configurations that free up certain pins for general use.
  If you are *absolutely* certain that your specific board design allows a normally-reserved pin to be used,
  you can suppress the error by setting this option to `true`. Defaults to `false`.

{{< anchor "config-time" >}}

## Time

In lots of places in ESPHome you need to define time periods.
There are several ways of doing this. See below examples to see how you can specify time periods:

```yaml
some_config_option:
  some_time_option: 1000us  # 1000 microseconds = 1ms
  some_time_option: 1000ms  # 1000 milliseconds
  some_time_option: 1.5s  # 1.5 seconds
  some_time_option: 0.5min  # half a minute
  some_time_option: 2h  # 2 hours

  # Make sure you wrap these in quotes
  some_time_option: '2:01'  # 2 hours 1 minute
  some_time_option: '2:01:30'  # 2 hours 1 minute 30 seconds

  # 10ms + 30s + 25min + 3h
  some_time_option:
    milliseconds: 10
    seconds: 30
    minutes: 25
    hours: 3
    days: 0

  # for all 'update_interval' options, also
  update_interval: never  # never update
  update_interval: 0ms  # update in every loop() iteration
  update_interval: always # same as 0ms
```

## See Also

- {{< docref "/index" "ESPHome index" >}}
- {{< docref "getting_started_command_line/" >}}
- {{< docref "faq/" >}}
