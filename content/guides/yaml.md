---
description: "YAML Configuration in ESPHome"
title: "YAML Configuration in ESPHome"
---

{{< anchor "yaml-configuration" >}}

## Overview

ESPHome configuration files use YAML, a human-friendly data serialization standard. This page explains both standard
YAML features and ESPHome-specific extensions.

YAML is designed to be human-readable and easy to edit, but can also be frustrating to work with, in particular when
it comes to indentation. YAML is a superset of JSON, and JSON syntax can be used in YAML files.

The order of configuration blocks within an ESPHome YAML file is generally unimportant, as the entire contents will
be read before any validation or processing is done.

## Standard YAML Features

- **Comments:** Any text after a `#` is a comment.
- **Scalars:** Strings, numbers, booleans.
- **Sequences:** Lists of items, using `-` or `[ ... ]`.
- **Mappings:** Key-value pairs, using `key: value` or `{ ... }`.
- **Anchors and Aliases:** Reuse blocks of YAML with `&anchor` and `*alias`.
- **Multi-line Strings:** Use `|` or `>` for multi-line text.

### Comments

A YAML comment is any text after a `#` symbol, extending to the end of the line. If you need to include a `#` character
in a string, it must appear within quotes.

Example:

```yaml
# this is a comment
foo:
  bar: 3 # this is another comment
  text: "# can be included in a string"
```

{{< anchor "yaml-scalars" >}}

### Scalars

A YAML scalar is any value that doesn't contain a colon (`:`). It can be a string, number, boolean, or null.

Strings are enclosed in double quotes (`"`) or single quotes (`'`). Standard escape sequences such as newline
(`\n`) and Unicode codepoints will be translated inside double quotes only. A string may also be an unquoted character
sequence that is not a valid number or boolean, for example `23times` will be treated as a string even if not quoted.
Strings may also be multi-line, using `|` or `>`.

Boolean values are `true` or `false`, case-insensitive. ESPHome also maps other strings to boolean values:

- `yes`, `on` and `enable` are mapped to `true`.
- `no`, `off` and `disable` are mapped to `false`.

Numeric values are integers or floating point numbers. Within ESPHome in most situations where a number is expected, it
can also be written as a string containing an integer or a floating point number which will be automatically converted.

Example:

```yaml
esp8266:
  board: esp8285    # esp8285 is a string
  restore_from_flash: true # boolean value

web_server:
  port: 80 # integer value
```

{{< anchor "yaml-sequences" >}}

### Sequences

A YAML sequence is a list (or array) of items, using `-` or `[ ... ]`. Items can be scalars, sequences, or mappings.
The `-` flag is used once per line for a sequence item, while the JSON style using `[ ... ]` can be on a single line,
or spread across multiple lines.

Example:

```yaml
# JSON style
data_pins: [48, 47, 39, 40]

# YAML style
data_pins:
  - 48
  - 47
  - 39
  - 40

sensors:
# A list of sensors, each is a mapping
  - platform: gpio
    name: "Temperature 1"
    pin: GPIO32

  - platform: gpio
    name: "Temperature 2"
    pin: GPIO33
```

Sequences in YAML format can be quite confusing at times - consider the following examples:

```yaml
- platform: gpio
  name: "Temperature 1"

- label:
    text: "Temperature 1"
```

It may seem odd that in the first case there is no additional indentation, while in the second case there is.
The difference is that in the first case the sequence item is itself a mapping, with keys `platform` and `name`,
while in the second case the sequence item is a key `label` with a value of a mapping with key `text` and value
`"Temperature 1"`. Rewriting these in JSON format can make it clearer:

```yaml
- {
    "platform": "gpio",
    "name": "Temperature 1"
  }
- {
    "label": {
      "text": "Temperature 1"
    }
  }
```

A useful rule of thumb is that wherever there is a sequence item that ends with a colon its value must be a mapping,
not a scalar, so it will require further indentation for the subsequent lines, This example is wrong and will throw
two errors:

```yaml
- label: # Will throw an error "expected a dictionary"
  text: "Temperature 1"  # Wrong! Should be indented. Will throw error "text is an invalid option for ..."
```

{{< anchor "yaml-mappings" >}}

### Mappings

A YAML mapping is a list of key-value pairs, using `key: value` or `{ ... }`. Keys can be any valid YAML scalar
(though usually they will be confined to strings from a predefined set), while values can be any valid YAML scalar,
list, or mapping. A mapping can also be referred to as a dictionary, associative array or hashtable. The keys used in
a single mapping must be unique.

Example:

```yaml
sensor:
  platform: gpio
  pin: GPIO32
  name: "Temperature 1"
  device_class: temperature
  unit_of_measurement: "°C"
  accuracy_decimals: 1
  state_class: measurement
```

In the example above "sensor" is a key in a mapping, and its value is another mapping. The second mapping has keys
`platform`, `pin`, `name`, `device_class`, `unit_of_measurement`, `accuracy_decimals` and `state_class`.

Where a mapping value is a sequence it should be indented after the key, but this is one of the few places that YAML
is forgiving of incorrect indentation, for example:

```yaml
widgets:
- label:
    text: Temperature 1
- label:
    text: Temperature 2
```

Note that the sequence marker `-` is *not* indented below the mapping key `widgets`. This technically incorrect,
but will be interpreted correctly by the YAML parser. It is recommended that you stick to the correct format,
but if you see this used in a YAML file, understand that it does work - and it can be useful to limit indentation
depth with complex configurations.

{{< anchor "yaml-anchors" >}}

### Anchors, Aliases, and Overriding Values

YAML anchors (`&anchor`  ) and aliases (`*alias`  ) allow you to define a block of configuration once and reuse it
elsewhere. This is especially useful for repeating metadata fields.
You can also override specific values when merging with `<<: *anchor`  :

```yaml
sensor:
  - &common_adc
      pin: GPIO32
      platform: adc
      name: "Temperature 1"
      device_class: temperature
      unit_of_measurement: "°C"
      accuracy_decimals: 1
      state_class: measurement

  - <<: *common_adc
    pin: GPIO33
    name: "Temperature 2"
```

In this example, both sensors share the metadata from `common_adc`, but the second sensor overrides the `pin` and
`name` values.

### Multi-line Strings

YAML supports multi-line strings in a few different flavors.

#### Quoted Multi-Line Strings

Strings that are quoted with double quotes (`"`  ) or single quotes (`'`  ) may be broken across lines. Points to note:

- Leading white space on subsequent lines is ignored;
- Newlines can be inserted by leaving a blank line;
- Escape sequences like `\n` are translated inside double quotes only;

Generally speaking block strings as described below are preferable to quoted multi-line strings.

Example:

```yaml
sensor:  # The name of this sensor will be "Sensor Name"
  - platform: template
    name: "Sensor
           Name"
```

#### Block Strings

Block strings are multi-line strings that are introduced with a special character sequence,
and all subsequent lines with indentation greater than the key introducing the string are considered part of the string.
There are three parts to a block string marker:

- The block style indicator (`|` or `>`  ) (required)
- The chomping indicator (`-` or `+`  ) (optional)
- An indentation value (a number, optional)

The block style controls how embedded newlines are handled - when using the `|` (literal) style,
embedded newlines are kept, while when using the `>` (folded) style, embedded newlines are folded into a single space.

The chomping indicator controls how the end of the string is treated:

- No chomping indicator: end the string with a single newline
- `-`  : remove all trailing newlines;
- `+`  : keep all trailing newlines.

The indentation value specifies how many spaces to insert at the beginning of each line. It is optional and the default
indentation will be guessed from the first line of text so in general it should not be necessary to use this.

Within ESPHome you will most often use the `|-` style which will keep internal newlines and remove trailing newlines.

Example:

```yaml
multiline_string: |-
  This is a string that is
  broken across multiple lines. Internal newlines
  will be kept, and trailing newlines will be removed.
some_other_key: # This is not part of the string
```

{{< anchor "yaml-extensions" >}}

## ESPHome YAML Extensions

ESPHome adds several non-standard but useful features to standard YAML:

{{< anchor "yaml-secrets" >}}

### Secrets and the `secrets.yaml` File

The `!secret` tag allows you to reference sensitive values (like passwords or API keys) stored in a separate
`secrets.yaml` file. This is especially helpful when you want to be able to distribute your configuration files
without revealing your secrets.

> [!IMPORTANT]
> In order to keep your secrets safe, the `secrets.yaml` file should NOT be checked into git or any other version
> control system.

Example:

```yaml
wifi:
  ssid: "MyWiFi"
  password: !secret wifi_password
```

And in your `secrets.yaml`

```yaml
wifi_password: my_super_secret_password
```

The secrets file must consist only of a flat mapping of keys to scalar values.

### Substitutions

The `substitutions:` feature allows you to define reusable values that can be referenced throughout your configuration.
For full details see {{< docref "/components/substitutions" >}}

{{< anchor "yaml-include" >}}

### !include

- Insert the contents of another YAML file at this position.
- May be used at any level of the configuration, and will be substituted at that level.
- Unless used in conjunction with `packages:` (see below) the insertion is done literally.
- Substitutions can be used in the included file to reference values passed to `!include`. Such values will override
  any global substitutions, so global substitutions can be used to provide default values.

Example:

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

### Packages

The `packages:` feature allows you to define reusable and potentially partial configurations that can be included in
your main configuration. The data is merged with the main configuration, with values in the main configuration taking
precedence over values in the package data.

See {{< docref "/components/packages" >}} for more details.

{{< anchor "yaml-hidden-items" >}}

### Hidden items

Any top-level configuration key that starts with a dot (`.`  ) will be ignored, and will not be included in the final
configuration. This is mostly useful to define anchors that are not part of the configuration.

```yaml
.number: &AnchorNumber # Define an anchor, but exclude it
    optimistic: true
    min_value: 0
    max_value: 600
    step: 1
    initial_value: 0

number:
  - platform: template
    <<: *AnchorNumber # Include the anchor previously defined
    id: "SwitchMainDelay"
    name: "Main Switch Delay"
```

The hidden key name is not important, and indeed can be just a single dot, but using a more descriptive name is
recommended.

### Lambdas

Within ESPHome configuration files it's possible to embed lambdas, which are blocks of C++ code that are evaluated
at runtime, to provide dynamic values and implement logic not possible in YAML. A lambda is defined using the
`!lambda` tag. See [Templates](/automations/templates#config-lambda) for more information.

## See Also

- {{< docref "/components/packages" >}}
- {{< docref "/guides/configuration-types" >}}
- [YAML Official Site](https://yaml.org/)
