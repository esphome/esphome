---
description: "Instructions for setting up a Tuya cover motor."
title: "Tuya Cover"
params:
  seo:
    description: Instructions for setting up a Tuya cover motor.
---

The `tuya` cover platform creates a simple position-only cover from a
tuya serial component.

This requires the {{< docref "/components/tuya" >}} component to be set up before you can use this platform.

Here is an example output for a Tuya M515EGWT (motor for chain roller blinds):

```text
[21:50:28][C][tuya:024]: Tuya:
[21:50:28][C][tuya:031]:   Datapoint 2: int value (value: 53)
[21:50:28][C][tuya:029]:   Datapoint 5: switch (value: OFF)
```

On this cover motor, the position control is datapoint 2.
Now you can create the cover.

```yaml
# Create a cover using the datapoint from above
cover:
  - platform: "tuya"
    name: "motor1"
    position_datapoint: 2
```

## Configuration variables

- **control_datapoint** (*Optional*, int): The datapoint id number for sending control commands.
- **position_datapoint** (**Required**, int): The datapoint id number of the cover position value.
- **position_report_datapoint** (*Optional*, int): The datapoint id number of the cover position report value, if separate from position_datapoint.
- **direction_datapoint** (*Optional*, int): The datapoint id number for setting the direction of travel.
- **min_value** (*Optional*, int): The lowest position value, meaning cover closed. Defaults to 0.
- **max_value** (*Optional*, int): the highest position value, meaning cover opened. Defaults to 255.
- **invert_position** (*Optional*, boolean): Sets the direction of travel to be inverted, if direction_datapoint is configured.
- **invert_position_report** (*Optional*, boolean): Invert reported position percentages calculated from `min_value` and `max_value` i.e. 70% becomes 30%. Defaults to false.
- All other options from [Cover](/components/cover#config-cover).

## Supported devices

Tuya cover devices known to be supported by this component:

- Tuya `M515EGWT` (motor for bead chain roller blinds)

  - Only the `position` datapoint (2) is used for this device.
  - Datapoint 5's function is not currently known.

- Zemismart `ZM79E-DT` and `YH002` (curtain motor)

  - Supported datapoints: `control` (1), `position` (2), `position_report` (3) and `direction` (5).
  - The direction of travel is persisted to the Tuya MCU, so doesn't need to be set if you've already configured it via the remote control.

If you have a Tuya cover device that isn't listed above, it may still work - but you'll need to determine which datapoints it uses
(and what their IDs are) for yourself.

## Restore modes

The default restore mode (`RESTORE`  ) attempts to restore the state on startup, but doesn't instruct the cover to move to that state.

`RESTORE_AND_CALL` additionally instructs the cover to move to the restored state - which might not work, depending on your device (see note below).

The Tuya MCU usually reports its position on startup, so `NO_RESTORE` will likely also appear to restore its state - but may take slightly longer.

Note that if your Tuya cover device uses relative position sensing (such as the ZM79E-DT), it can't tell if the cover was moved while not powered up.
This means that moving the cover while the device is powered off will result in its position not matching the reported/requested state.
In this condition, it will go into an error / uncalibrated state when it next tries to go in one direction (as it can't move as far as it wants to), requiring an open/close cycle to recalibrate.

## See Also

- {{< docref "/components/tuya" >}}
- {{< docref "/components/cover" >}}
- {{< apiref "tuya/cover/tuya_cover.h" "tuya/cover/tuya_cover.h" >}}
