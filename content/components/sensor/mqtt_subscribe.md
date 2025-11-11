---
description: "Instructions for setting up MQTT subscribe sensors"
title: "MQTT Subscribe Sensor"
params:
  seo:
    description: Instructions for setting up MQTT subscribe sensors
    image: mqtt.png
---

The `mqtt_subscribe` sensor platform allows you to get external data into ESPHome.
The sensor will subscribe to messages on the given MQTT topic and parse each message into
a floating point number.

Please note this component only works with MQTT topics that have numeric data! Each time
a message that is not a number is published a warning will be shown. Please use the MQTT subscribe
text sensor for importing arbitrary text into the ESPHome ecosystem.

```yaml
# Example configuration entry
sensor:
  - platform: mqtt_subscribe
    name: "Data from topic"
    id: mysensor
    topic: the/topic
```

## Configuration variables

- **topic** (**Required**, string): The MQTT topic to listen for numeric messages.
- **qos** (*Optional*, int): The MQTT QoS to subscribe with. Defaults to `0`.
- All other options from [Sensor](/components/sensor).

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- {{< apiref "mqtt_subscribe/sensor/mqtt_subscribe_sensor.h" "mqtt_subscribe/sensor/mqtt_subscribe_sensor.h" >}}
