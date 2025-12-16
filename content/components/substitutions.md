---
description: "How to use substitutions in ESPHome"
title: "Substitutions"
params:
  seo:
    description: How to use substitutions in ESPHome
    image: settings.svg
---

ESPHome has a powerful way to reduce repetition in configuration files: substitutions.
With substitutions, you can have a single generic source file for all nodes of one kind and
substitute expressions in as required.

```yaml
substitutions:
  bme280_temperature_offset: "-1.0"

sensor:
  - platform: bme280_i2c
    temperature:
      name: BME280 Temperature
      filters:
        - offset: ${bme280_temperature_offset}
```

In the top-level `substitutions` section, you can put as many key-value pairs as you want. Before
validating your configuration, ESPHome will automatically replace all occurrences of substitutions
by their value. The syntax for a substitution is based on bash and is case-sensitive: `$substitution_key` or
`${substitution_key}` (same).

Substitution variables can be of any valid YAML type, for example:

```yaml
substitutions:
  device:
    name: Kitchen AC
    port: 12
    enabled: true
  color: "yellow"
  unused_pins: [12, 23, 27]
```

Two substitution passes are performed allowing compound replacements:

```yaml
substitutions:
  foo: yellow
  bar_yellow_value: !secret yellow_secret
  bar_green_value: !secret green_secret

something:
  test: ${bar_${foo}_value}
```

The above is supported for backward compatibility. It is recommended that
you use key-value dictionaries going forward:

```yaml
substitutions:
  foo: yellow
  bar:
    yellow: !secret yellow_secret
    green: !secret green_secret

something:
  test: ${bar[foo]}
```

{{< anchor "jinja-expressions" >}}

## Jinja expressions

Simple Jinja expressions and filters can be used inside `${ ... }` syntax.

All substitution variables become accessible within Jinja expressions by their name.

If the substitution variable is a key-value dictionary, you can access members with a dot notation: `${ device.name }`,
or indexed `${ device["name"] }`

Lists can be indexed: `${ unused_pins[2] }`

```yaml
substitutions:
  native_width: 480
  native_height: 320
  high_dpi: true
  scale: 1.5
  sensor_pin:
    number: 3
    inverted: true
  debug_label:
    width: 200
    height: 20
    enabled: true

display:
  - platform: ili9xxx
    dimensions:
      width: ${native_width * 2 if high_dpi else native_width}
      height: ${native_height * 2 if high_dpi else native_height}

lvgl:
  widgets:
    - label:
        id: debug_info
        hidden: ${not debug_label.enabled}
        width: ${ (debug_label.width * scale) | round | int }
        height: ${ (debug_label.height * scale) | round | int }
        text: |
          High DPI is ${high_dpi and "enabled" or "disabled"}.

binary_sensor:
  - platform: gpio
    name: Binary sensor on pin ${sensor_pin.number}
    pin: ${sensor_pin}
```

Note that in other projects Jinja uses the `{{ ... }}` syntax for expression delimiters.
In ESPHome we have configured Jinja to use `${...}` instead, so it is the same as the
existing substitution syntax and to avoid conflicts with Home Assistant's own use of Jinja.

To understand what types of expressions and filters can be used,
refer to [Jinja Expressions](https://jinja.palletsprojects.com/en/stable/templates/#expressions) documentation.

### Mathematical operations

In addition to Jinja's native operators such as `+`, `-`, `*`, `/`, ... Python's math
library is exposed as a module:

```yaml
substitutions:
  x: 20
  y: 50
lvgl:
  widgets:
    - label:
        x: $x
        y: $y
        text: Distance is ${math.sqrt(x*x+y*y)}.
```

To see what mathematical functions are available,
refer to [Python math library](https://docs.python.org/3/library/math.html) documentation.

### Built-in functions

In addition to the Jinja expressions, ESPHome supports a number of built-in functions that can be used in substitutions.

- `ord` Returns the Unicode code point for a given character. Example: `ord("A") == 65`
- `chr` Returns the character for a given Unicode code point. Example: `chr(65) == "A"`
- `len` Returns the length of the string. Example: `len("Hello") == 5`

{{< anchor "substitute-include-variables" >}}

## Disabling Jinja and substitutions

You can prevent ESPHome from substituting variables or processing Jinja by means of the `!literal` tag before any value:

```yaml
substitutions:
  value: "Test Value"
lvgl:
  widgets:
    - label:
        text: !literal "This is a ${value}"
```

In the above example, the value of the `text` property will be, literally, `This is a ${value}`.

## Substitute !include variables

ESPHome's `!include` accepts a list of variables that can be substituted within the included file.

```yaml
binary_sensor:
  - platform: gpio
    id: button1
    pin: GPIOXX
    on_multi_click: !include { file: on-multi-click.yaml, vars: { id: 1 } } # inline syntax
  - platform: gpio
    id: button2
    pin: GPIOXX
    on_multi_click: !include
      # multi-line syntax
      file: on-multi-click.yaml
      vars:
        id: 2
```

`on-multi-click.yaml`  :

```yaml
- timing: !include click-single.yaml
  then:
    - mqtt.publish:
        topic: ${device_name}/button${id}/status
        payload: single
- timing: !include click-double.yaml
  then:
    - mqtt.publish:
        topic: ${device_name}/button${id}/status
        payload: double
```

{{< anchor "command-line-substitutions" >}}

## Command line substitutions

You can define or override substitutions from the command line by adding the `-s` switch with arguments `KEY` and
`VALUE`. This will override the substitution `KEY` and assign it the value `VALUE`. This switch can be included
multiple times. Consider the following `example.yaml` file:

```yaml
substitutions:
  name: my_default_name

esphome:
  name: $name
```

...and the following command:

```bash
esphome -s name my_device01 config example.yaml
```

You will get something like the following output:

```yaml
substitutions:
  name: my_device01

esphome:
  name: my_device01
  # ...
```

Command line substitutions take precedence over those in your configuration file. This can be used to create generic
"template" configuration files (like `example.yaml` above) which can be used by multiple devices, leveraging
substitutions which are provided on the command line.

{{< anchor "yaml-insertion-operator" >}}

## Bonus: YAML insertion operator

Additionally, you can use the YAML insertion operator `<<` syntax to create a single YAML file from which a number
of nodes inherit:

```yaml
# In common.yaml
esphome:
  name: $devicename
  # ...

sensor:
- platform: dht
  # ...
  temperature:
    name: Temperature
  humidity:
    name: Humidity
```

```yaml
# In nodemcu1.yaml
substitutions:
  devicename: nodemcu1

<<: !include common.yaml
```

> [!TIP]
> To hide these base files from the dashboard, you can
>
> - Place them in a subdirectory (dashboard only shows files in top-level directory)
> - Prepend a dot to the filename, like `.base.yaml`

## See Also

- {{< docref "/index" "ESPHome index" >}}
- {{< docref "/guides/getting_started_command_line" >}}
- {{< docref "/guides/faq" >}}
