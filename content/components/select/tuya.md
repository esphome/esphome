---
description: "Instructions for setting up a Tuya device select."
title: "Tuya Select"
params:
  seo:
    description: Instructions for setting up a Tuya device select.
    image: tuya.png
---

The `tuya` select platform creates a select from a tuya serial component
and requires {{< docref "/components/tuya" >}} to be configured.

```text
[08:51:09][C][tuya:032]: Tuya:
[08:51:09][C][tuya:043]:   Datapoint 1: switch (value: ON)
[08:51:09][C][tuya:045]:   Datapoint 24: int value (value: 220)
[08:51:09][C][tuya:045]:   Datapoint 16: int value (value: 22)
[08:51:09][C][tuya:049]:   Datapoint 2: enum (value: 1)
[08:51:09][C][tuya:045]:   Datapoint 19: int value (value: 40)
[08:51:09][C][tuya:045]:   Datapoint 101: int value (value: 1)
[08:51:09][C][tuya:045]:   Datapoint 27: int value (value: -2)
[08:51:09][C][tuya:049]:   Datapoint 43: enum (value: 1)
[08:51:09][C][tuya:049]:   Datapoint 102: enum (value: 1)
[08:51:09][C][tuya:051]:   Datapoint 45: bitmask (value: 0)
[08:51:09][C][tuya:043]:   Datapoint 10: switch (value: ON)
[08:51:09][C][tuya:041]:   Datapoint 38: raw (value: 06.00.14.08.00.0F.0B.1E.0F.0C.1E.0F.11.00.16.16.00.0F.08.00.16.17.00.0F (24))
[08:51:09][C][tuya:049]:   Datapoint 36: enum (value: 1)
[08:51:09][C][tuya:057]:   GPIO Configuration: status: pin 14, reset: pin 0 (not supported)
[08:51:09][C][tuya:061]:   Status Pin: GPIO14
[08:51:09][C][tuya:063]:   Product: '{"p":"gogb05wrtredz3bs","v":"1.0.0","m":0}'
```

On this controller, the datapoint 36 represents the temperature sensor selection
setting which is what we are interested in controlling using this platform.

Based on this, you can create the select as follows:

```yaml
# Create a select
select:
  - platform: "tuya"
    name: "Sensor selection"
    enum_datapoint: 2
    optimistic: true
    options:
      0: Internal
      1: Floor
      2: Both
```

## Configuration variables

- **enum_datapoint** (**Required**, int): The enum datapoint id number for the select.
  At least one of *enum_datapoint* or *int_datapoint* is required.

- **int_datapoint** (**Required**, int): The int datapoint id number for the select.
  At least one of *enum_datapoint* or *int_datapoint* is required.

- **options** (**Required**, Map[int, str]): Provide a mapping from values (int) of
  this Select to options (str) of the *enum_datapoint* and vice versa. All options and
  all values have to be unique.

- **optimistic** (*Optional*, boolean): Whether to operate in optimistic mode - when in this mode,
  any command sent to the Select will immediately update the reported state.

- All other options from [Select](/components/select#config-select).

## See Also

- {{< docref "/components/select" >}}
- {{< apiref "tuya/select/tuya_select.h" "tuya/select/tuya_select.h" >}}
