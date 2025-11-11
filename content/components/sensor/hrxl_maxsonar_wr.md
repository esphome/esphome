---
description: "Instructions for setting up MaxBotix HRXL or XL MaxSonar WR ultrasonic distance measurement sensors in ESPHome."
title: "HRXL/XL MaxSonar WR Series"
params:
  seo:
    description: Instructions for setting up MaxBotix HRXL or XL MaxSonar WR ultrasonic distance measurement sensors in ESPHome.
    image: hrxl_maxsonar_wr.jpg
---

This sensor allows you to use HRXL MaxSonar WR series ultrasonic sensors by MaxBotix
([datasheet](https://www.maxbotix.com/documents/HRXL-MaxSonar-WR_Datasheet.pdf))
or the XL MaxSonar WR series
([datasheet](https://www.maxbotix.com/documents/XL-MaxSonar-WR_Datasheet.pdf))
with ESPHome to measure distances. Depending on the model, these sensors can measure
in a range between 30 centimeters and 10 meters.

This sensor platform works with the **TTL versions** of those sensors and expects the
sensor's TTL pin to be wired to one of the ESP's input pins. Since these sensors read
multiple times per second, filtering is highly recommended.

{{< img src="hrxl_maxsonar_wr-full.jpg" alt="Image" caption="MB7388 HRXL-MaxSonar-WRMLT Ultrasonic Distance Sensor." width="50.0%" class="align-center" >}}

```yaml
# Example configuration entry
sensor:
  - platform: "hrxl_maxsonar_wr"
    name: "Rainwater Tank"
```

## Configuration variables

- All options from [Sensor](/components/sensor).

Advanced options:

- **uart_id** (*Optional*, [ID](/guides/configuration-types#id)): The ID of the [UART bus](/components/uart) you wish to use for this sensor.
  Use this if you want to use multiple UART buses at once.

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- [UART Bus](/components/uart)
- {{< docref "template/" >}}
- {{< apiref "hrxl_maxsonar_wr/hrxl_maxsonar_wr.h" "hrxl_maxsonar_wr/hrxl_maxsonar_wr.h" >}}
