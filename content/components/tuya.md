---
description: "Instructions for setting up the Tuya component."
title: "Tuya MCU"
params:
  seo:
    description: Instructions for setting up the Tuya component.
    image: tuya.png
---

The `tuya` component creates a serial connection to the Tuya MCU for platforms to use.

{{< img src="tuya.png" alt="Image" width="40%" class="align-center" >}}

The `tuya` serial component requires a [UART bus](/components/uart) to be configured.
Put the `tuya` component in the config and it will list the possible devices for you in the config log.

```yaml
# Register the Tuya MCU connection
tuya:
```

Here is an example output for a Tuya fan controller:

```text
[12:39:45][C][tuya:023]: Tuya:
[12:39:45][C][tuya:032]:   Datapoint 1: switch (value: ON)
[12:39:45][C][tuya:036]:   Datapoint 3: enum (value: 1)
[12:39:45][C][tuya:036]:   Datapoint 6: enum (value: 0)
[12:39:45][C][tuya:034]:   Datapoint 7: int value (value: 0)
[12:39:45][C][tuya:032]:   Datapoint 9: switch (value: OFF)
[12:39:45][C][tuya:046]:   Product: '{"p":"hqq73kftvzh8c92u","v":"1.0.0","m":0}'
```

Here is another example output for a Tuya ME-81H thermostat:

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

## Configuration variables

- **time_id** (*Optional*, [ID](/guides/configuration-types#id)): Some Tuya devices support obtaining local time from ESPHome.
  Specify the ID of the {{< docref "time/" >}} which will be used.

- **status_pin** (*Optional*, [Pin Schema](/guides/configuration-types#pin-schema)): Some Tuya devices support WiFi status reporting ONLY
  through gpio pin.
  Specify the pin reported in the config dump or leave empty otherwise.
  More about this on the [Tuya Developer Documentation](https://developer.tuya.com/en/docs/iot/tuya-cloud-universal-serial-port-access-protocol?id=K9hhi0xxtn9cb#title-6-Query%20working%20mode).

- **ignore_mcu_update_on_datapoints** (*Optional*, list): A list of datapoints to ignore MCU updates for. Useful for
  certain broken/erratic hardware and debugging.

Automations:

- **on_datapoint_update** (*Optional*): An automation to perform when a Tuya datapoint update is received. See [`on_datapoint_update`](#tuya-on_datapoint_update).

## Tuya Automation

{{< anchor "tuya-on_datapoint_update" >}}

### `on_datapoint_update`

This automation will be triggered when a Tuya datapoint update is received.
A variable `x` is passed to the automation for use in lambdas.
The type of `x` variable is depending on `datapoint_type` configuration variable:

- *raw*: `x` is `std::vector<uint8_t>`
- *string*: `x` is `std::string`
- *bool*: `x` is `bool`
- *int*: `x` is `int`
- *uint*: `x` is `uint32_t`
- *enum*: `x` is `uint8_t`
- *bitmask*: `x` is `uint32_t`
- *any*: `x` is {{< apistruct "tuya::TuyaDatapoint" "tuya::TuyaDatapoint" >}}

```yaml
tuya:
  on_datapoint_update:
    - sensor_datapoint: 6
      datapoint_type: raw
      then:
        - lambda: |-
            ESP_LOGD("main", "on_datapoint_update %s", format_hex_pretty(x).c_str());
            id(voltage).publish_state((x[0] << 8 | x[1]) * 0.1);
            id(current).publish_state((x[3] << 8 | x[4]) * 0.001);
            id(power).publish_state((x[6] << 8 | x[7]) * 0.1);
    - sensor_datapoint: 7 # sample dp
      datapoint_type: string
      then:
        - lambda: |-
            ESP_LOGD("main", "on_datapoint_update %s", x.c_str());
    - sensor_datapoint: 8 # sample dp
      datapoint_type: bool
      then:
        - lambda: |-
            ESP_LOGD("main", "on_datapoint_update %s", ONOFF(x));
    - sensor_datapoint: 6
      datapoint_type: any # this is optional
      then:
        - lambda: |-
            if (x.type == tuya::TuyaDatapointType::RAW) {
              ESP_LOGD("main", "on_datapoint_update %s", format_hex_pretty(x.value_raw).c_str());
            } else {
              ESP_LOGD("main", "on_datapoint_update %hhu", x.type);
            }
```

### Configuration variables

- **sensor_datapoint** (**Required**, int): The datapoint id number of the sensor.
- **datapoint_type** (**Required**, string): The datapoint type one of *raw*, *string*, *bool*, *int*, *uint*, *enum*,
  *bitmask* or *any*.
- See [Automation](/automations).

## See Also

- {{< docref "/components/fan/tuya" >}}
- {{< docref "/components/light/tuya" >}}
- {{< docref "/components/switch/tuya" >}}
- {{< docref "/components/climate/tuya" >}}
- {{< docref "/components/binary_sensor/tuya" >}}
- {{< docref "/components/sensor/tuya" >}}
- {{< docref "/components/text_sensor/tuya" >}}
- {{< docref "/components/number/tuya" >}}
- {{< apiref "tuya/tuya.h" "tuya/tuya.h" >}}
