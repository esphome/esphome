---
description: "Instructions for setting up a Tuya device switch."
title: "Tuya Switch"
params:
  seo:
    description: Instructions for setting up a Tuya device switch.
    image: upload.svg
---

The `tuya` switch platform creates a sensor from a tuya serial component
and requires {{< docref "/components/tuya" >}} to be configured.

```text
[13:46:01][C][tuya:023]: Tuya:
[13:46:01][C][tuya:032]:   Datapoint 1: switch (value: OFF)
[13:46:01][C][tuya:032]:   Datapoint 2: switch (value: OFF)
[13:46:01][C][tuya:034]:   Datapoint 3: int value (value: 19)
[13:46:01][C][tuya:034]:   Datapoint 4: int value (value: 17)
[13:46:01][C][tuya:034]:   Datapoint 5: int value (value: 0)
[13:46:01][C][tuya:036]:   Datapoint 7: enum (value: 1)
[13:46:01][C][tuya:046]:   Product: '{"p":"ynjanlglr4qa6dxf","v":"1.0.0","m":0}'
```

On this controller, the datapoint 2 represents the child lock switch
setting which is what we are interested in controlling using this platform.

Based on this, you can create the switch as follows:

```yaml
# Create a switch
switch:
  - platform: "tuya"
    name: "MySwitch"
    switch_datapoint: 2
```

## Configuration variables

- **switch_datapoint** (**Required**, int): The datapoint id number of the switch.
- All other options from [Switch](/components/switch#config-switch).

## See Also

- {{< docref "/components/switch" >}}
- {{< apiref "tuya/switch/tuya_switch.h" "tuya/switch/tuya_switch.h" >}}
