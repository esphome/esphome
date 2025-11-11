---
description: "Instructions for setting up the copy component in ESPHome"
title: "Copy Component"
params:
  seo:
    description: Instructions for setting up the copy component in ESPHome
    image: content-copy.svg
---

The `copy` component can be used to copy an existing component (like a sensor, switch, etc.)
and create a duplicate mirroring the source's state and forwarding actions such as turning on to the source.

For each of the supported platforms, the configuration consists of the required configuration
variable `source_id`, which is used to indicate the source of the object being mirorred.

## Copy Binary Sensor

```yaml
# Example configuration entry
binary_sensor:
  - platform: copy
    source_id: source_binary_sensor
    name: "Copy of source_binary_sensor"
```

### Configuration variables

- **source_id** (**Required**, [ID](/guides/configuration-types#id)): The binary sensor that should be mirrored.
- All other options from [Binary Sensor](/components/binary_sensor#config-binary_sensor).

## Copy Button

```yaml
# Example configuration entry
button:
  - platform: copy
    source_id: source_button
    name: "Copy of source_button"
```

### Configuration variables

- **source_id** (**Required**, [ID](/guides/configuration-types#id)): The button that should be mirrored.
- All other options from [Button](/components/button#config-button).

## Copy Cover

```yaml
# Example configuration entry
cover:
  - platform: copy
    source_id: source_cover
    name: "Copy of source_cover"
```

### Configuration variables

- **source_id** (**Required**, [ID](/guides/configuration-types#id)): The cover that should be mirrored.
- All other options from [Cover](/components/cover#config-cover).

## Copy Fan

```yaml
# Example configuration entry
fan:
  - platform: copy
    source_id: source_fan
    name: "Copy of source_fan"
```

### Configuration variables

- **source_id** (**Required**, [ID](/guides/configuration-types#id)): The fan that should be mirrored.
- All other options from [Fan](/components/fan#config-fan).

## Copy Lock

```yaml
# Example configuration entry
lock:
  - platform: copy
    source_id: source_lock
    name: "Copy of source_lock"
```

### Configuration variables

- **source_id** (**Required**, [ID](/guides/configuration-types#id)): The lock that should be mirrored.
- All other options from [Lock](/components/lock#config-lock).

## Copy Number

```yaml
# Example configuration entry
number:
  - platform: copy
    source_id: source_number
    name: "Copy of source_number"
```

### Configuration variables

- **source_id** (**Required**, [ID](/guides/configuration-types#id)): The number that should be mirrored.
- All other options from [Number](/components/number#config-number).

## Copy Select

```yaml
# Example configuration entry
select:
  - platform: copy
    source_id: source_select
    name: "Copy of source_select"
```

### Configuration variables

- **source_id** (**Required**, [ID](/guides/configuration-types#id)): The select that should be mirrored.
- All other options from [Select](/components/select#config-select).

{{< anchor "copy-sensor" >}}

## Copy Sensor

```yaml
# Example configuration entry
sensor:
  - platform: copy
    source_id: source_sensor
    name: "Copy of source_sensor"
```

### Configuration variables

- **source_id** (**Required**, [ID](/guides/configuration-types#id)): The sensor that should be mirrored.
- All other options from [Sensor](/components/sensor).

## Copy Switch

```yaml
# Example configuration entry
switch:
  - platform: copy
    source_id: source_switch
    name: "Copy of source_switch"
```

### Configuration variables

- **source_id** (**Required**, [ID](/guides/configuration-types#id)): The switch that should be mirrored.
- All other options from [Switch](/components/switch#config-switch).

## Copy Text Sensor

```yaml
# Example configuration entry
text_sensor:
  - platform: copy
    source_id: source_text_sensor
    name: "Copy of source_text_sensor"
```

### Configuration variables

- **source_id** (**Required**, [ID](/guides/configuration-types#id)): The text sensor that should be mirrored.
- All other options from [Text Sensor](/components/text_sensor#config-text_sensor).

## Copy Text

```yaml
# Example configuration entry
text:
  - platform: copy
    source_id: source_text
    name: "Copy of source_text"
```

### Configuration variables

- **source_id** (**Required**, [ID](/guides/configuration-types#id)): The text that should be mirrored.
- All other options from [Text](/components/text#config-text).

## See Also
