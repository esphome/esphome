---
description: "Instructions for setting up a Tuya device integer or enum datapoint.."
title: "Tuya Number"
params:
  seo:
    description: Instructions for setting up a Tuya device integer or enum datapoint..
    image: upload.svg
---

The `tuya` number platform allows you to create a number that controls
a tuya serial component. This platform requires {{< docref "/components/tuya" >}} to be configured.

When {{< docref "/components/tuya" >}} has been properly configured, it will output a list of
valid data points to the log after start-up.

```text
[21:37:14][C][tuya:028]: Tuya:
[21:37:14][C][tuya:045]:   Datapoint 101: enum (value: 4)
[21:37:14][C][tuya:045]:   Datapoint 102: enum (value: 1)
[21:37:14][C][tuya:041]:   Datapoint 103: int value (value: 5)
[21:37:14][C][tuya:039]:   Datapoint 104: switch (value: OFF)
[21:37:14][C][tuya:041]:   Datapoint 105: int value (value: 229)
[21:37:14][C][tuya:041]:   Datapoint 106: int value (value: 37)
[21:37:14][C][tuya:041]:   Datapoint 107: int value (value: 10)
[21:37:14][C][tuya:041]:   Datapoint 108: int value (value: 35)
[21:37:14][C][tuya:041]:   Datapoint 109: int value (value: 30)
[21:37:14][C][tuya:041]:   Datapoint 110: int value (value: 80)
[21:37:14][C][tuya:039]:   Datapoint 112: switch (value: OFF)
[21:37:14][C][tuya:039]:   Datapoint 113: switch (value: OFF)
[21:37:14][C][tuya:039]:   Datapoint 114: switch (value: OFF)
[21:37:14][C][tuya:045]:   Datapoint 115: enum (value: 4)
[21:37:14][C][tuya:045]:   Datapoint 116: enum (value: 2)
[21:37:14][C][tuya:055]:   Product: '{"p":"ymf4oruxqx0xlogp","v":"1.0.3","m":0}'
```

The example output above from a Tuya Siren with temperature and humidity sensors. The
`tuya` number platform can be used to control all of the integer and enum datapoints.

On this device, datapoint 116 represents the volume control, with valid values being
0=High, 1=Medium, 2=Low.

Based on this, you can create a number as follows:

```yaml
- platform: "tuya"
  name: "Volume"
  number_datapoint: 116
  min_value: 0
  max_value: 2
  step: 1
```

The value for `multiply` is used as the scaling factor for the Number. All numbers in Tuya are integers, so a scaling factor is sometimes needed to convert the Tuya reported value into floating point.

For instance, assume we have a pH sensor that reads from 0.00 to 15.00 with a scaling of 0.01. By setting `multiply` to 100, on the Tuya side (not visible to the user) the number will be reported as an integer from 0 to 1500. The following configuration could be used:

```yaml
- platform: "tuya"
  name: "pH Sensor"
  number_datapoint: 106
  min_value: 0.00
  max_value: 15.00
  multiply: 100
```

## Hidden datapoints

The above configurations will work fine as long as Tuya device publishes the datapoint value (along with its type) at initialization.
However this is not always the case. To be able to use such "hidden" datapoints as Number, you need to specify additional `datapoint_hidden` configuration block.
This block allows to specify the missing datapoint type and, optionally, the value that should be written to the datapoint at initialization.

TuyaMCU restores the state of all its datapoints after reboot, but with the hidden datapoints there is no way to know what their values are.
Therefore there is also an option to store them on the ESPHome side and they will be set at initialization. To use this feature, set the `restore_value` yaml key to True.

```yaml
- platform: "tuya"
  name: "Alarm at maximum"
  number_datapoint: 116
  min_value: 0
  max_value: 100
  datapoint_hidden:
    datapoint_type: int
    initial_value: 85
    restore_value: yes
```

## Configuration variables

- **number_datapoint** (**Required**, int): The datapoint id number of the number.
- **min_value** (**Required**, float): The minimum value this number can be.
- **max_value** (**Required**, float): The maximum value this number can be.
- **step** (*Optional*, float): The granularity with which the number can be set. Defaults to 1.
- **multiply** (*Optional*, float): multiply the new value with this factor before sending the requests.
- **datapoint_hidden** (*Optional*): Specify information required for hidden datapoints.

  - **datapoint_type** (**Required**, string): The datapoint type, one of *int*, *uint*, *enum*.
  - **initial_value** (*Optional*, float): The value to be written at initialization. Must be between `min_value` and `max_value`.
  - **restore_value** (*Optional*, boolean): Saves and loads the state to RTC/Flash. Defaults to `false`.

- All other options from [Number](/components/number#config-number).

## See Also

- {{< docref "/components/number" >}}
- {{< apiref "tuya/number/tuya_number.h" "tuya/number/tuya_number.h" >}}
