---
description: "Instructions for setting up SCD30 CO₂ Temperature and Relative Humidity Sensor"
title: "SCD30 CO₂, Temperature and Relative Humidity Sensor"
params:
  seo:
    description: Instructions for setting up SCD30 CO₂ Temperature and Relative Humidity Sensor
    image: scd30.jpg
---

The `scd30` sensor platform allows you to use your Sensirion SCD30 CO₂
([datasheet](https://sensirion.com/media/documents/4EAF6AF8/61652C3C/Sensirion_CO2_Sensors_SCD30_Datasheet.pdf)) sensors with ESPHome.
The [I²C Bus](/components/i2c) is required to be set up in your configuration for this sensor to work.

{{< img src="scd30.jpg" alt="Image" width="80.0%" class="align-center" >}}

```yaml
# Example configuration entry
sensor:
  - platform: scd30
    co2:
      name: "Workshop CO2"
      accuracy_decimals: 1
    temperature:
      name: "Workshop Temperature"
      accuracy_decimals: 2
    humidity:
      name: "Workshop Humidity"
      accuracy_decimals: 1
    temperature_offset: 1.5 °C
    address: 0x61
    update_interval: 5s
```

## Configuration variables

- **co2** (*Optional*): The information for the CO₂ sensor.

  - All options from [Sensor](/components/sensor).

- **temperature** (*Optional*): The information for the Temperature sensor.

  - All options from [Sensor](/components/sensor).

- **humidity** (*Optional*): The information for the Humidity sensor.

  - All options from [Sensor](/components/sensor).

- **temperature_offset** (*Optional*, float): Temperature and humidity
  offsets may occur when operating the sensor in end-customer
  devices. This variable allows the compensation of those effects by
  setting a temperature offset.

- **automatic_self_calibration** (*Optional*, boolean): Whether to enable
  automatic self calibration (ASC). Defaults to `true`.

- **ambient_pressure_compensation** (*Optional*, int): Enable compensation
  of measured CO₂ values based on given ambient pressure in mBar.

- **altitude_compensation** (*Optional*, int): Enable compensating
  deviations due to current altitude (in metres). Notice: setting
  *altitude_compensation* is ignored if *ambient_pressure_compensation*
  is set.

- **address** (*Optional*, int): Manually specify the I²C address of the sensor.
  Defaults to `0x61`.

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to check the
  sensor. Available range: [2 … 1800]. Defaults to `60s`.

## Manual calibration

```yaml
# Example on how to implement a UI section in HA for manual calibration.
# Note: Please enter first a CO2 value before pressing the button.
button:
  - platform: template
    name: "SCD30 Force manual calibration"
    entity_category: "config"
    on_press:
      then:
        - scd30.force_recalibration_with_reference:
            value: !lambda 'return id(co2_cal).state;'

number:
  - platform: template
    name: "CO2 calibration value"
    optimistic: true
    min_value: 350
    max_value: 4500
    step: 1
    id: co2_cal
    icon: "mdi:molecule-co2"
    entity_category: "config"
```

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- {{< docref "absolute_humidity/" >}}
- {{< docref "dht/" >}}
- {{< docref "dht12/" >}}
- {{< docref "hdc1080/" >}}
- {{< docref "htu21d/" >}}
- {{< apiref "scd30/scd30.h" "scd30/scd30.h" >}}
