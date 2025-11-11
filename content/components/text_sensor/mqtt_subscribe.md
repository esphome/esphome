---
description: "Instructions for setting up MQTT Subscribe text sensors that show the content of a MQTT message as their state."
title: "MQTT Subscribe Text Sensor"
params:
  seo:
    description: Instructions for setting up MQTT Subscribe text sensors that show the content of a MQTT message as their state.
    image: mqtt.png
---

The `mqtt_subscribe` text sensor platform allows you to get external data into ESPHome.
The sensor will subscribe to messages on the given MQTT topic and save the most recent value
in its `id(mysensor).state`.

```yaml
# Example configuration entry
text_sensor:
  - platform: mqtt_subscribe
    name: "Data from topic"
    id: mysensor
    topic: the/topic
```

## Configuration variables

- **topic** (**Required**, string): The MQTT topic to listen for string data.
- **qos** (*Optional*, int): The MQTT QoS to subscribe with. Defaults to `0`.
- All other options from [Text Sensor](/components/text_sensor#config-text_sensor).

## Example Usage for Displays

This component is especially useful for displays, to show external data on the display.
Please note you have to use the `.c_str()` method on the `.state` object together with the `%s` format
to use it in `printf` expressions.

```yaml
# Example configuration entry
text_sensor:
  - platform: mqtt_subscribe
    name: "Data from topic"
    id: mysensor
    topic: the/topic

display:
  - platform: ...
    # ...
    lambda: |-
      it.printf(0, 0, id(font), "The data is: %s", id(mysensor).state.c_str());
```

## See Also

- {{< apiref "mqtt_subscribe/text_sensor/mqtt_subscribe_text_sensor.h" "mqtt_subscribe/text_sensor/mqtt_subscribe_text_sensor.h" >}}
