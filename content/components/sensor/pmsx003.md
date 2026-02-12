---
description: "Instructions for setting up PMSX003 Particulate matter sensors"
title: "PMSX003 Particulate Matter Sensor"
params:
  seo:
    description: Instructions for setting up PMSX003 Particulate matter sensors
    image: pmsx003.svg
---

The `pmsx003` sensor platform allows you to use your [Plantower](https://www.plantower.com/en/products_33/)
[PMS1003](https://www.plantower.com/static/upload/file/20220627/1656292073878896.pdf),
[PMS3003](https://www.dynamoelectronics.com/descargas/PMS3003.pdf),
[PMS5003](https://www.aqmd.gov/docs/default-source/aq-spec/resources-page/plantower-pms5003-manual_v2-3.pdf),
PMS5003S,
PMS5003T,
[PMS5003ST](https://raw.githubusercontent.com/Arduinolibrary/DFRobot_SEN0233_Air_Quality_Monitor/master/PMS5003ST%20series%20data%20manua_English_V2.6%20.pdf),
[PMS6003](https://www.laskakit.cz/user/related_files/203-pms6003.pdf),
[PMS7003](https://download.kamami.pl/p564008-PMS7003%20series%20data%20manua_English_V2.5.pdf),
[PMS9003M](https://evelta.com/content/datasheets/203-PMS9003M.pdf),
[PMSA003](https://evelta.com/content/datasheets/PMSA003%20series%20data%20manual_English_V2.5.pdf),
laser based particulate matter sensors with ESPHome.

As the communication with the PMSX003 is done using UART, you need
to have an [UART bus](/components/uart) in your configuration with the `rx_pin` connected to the SEND/TX pin
(may also be called the RX pin, depending on the model) of the PMS. Additionally, you need to set the baud rate to 9600.

This platform supports multiple sensor types, which you need to specify using the `type:` configuration
value.

| Type         | `PMS1003` | `PMS3003` | `PMSX003`                                  | `PMS5003S` | `PMS5003T` | `PMS5003ST` | `PMS9003M` |
| ------------ | --------- | --------- | ------------------------------------------ | ---------- | ---------- | ----------- | ---------- |
| Model        | `PMS1003` | `PMS3003` | `PMS5003`, `PMS6003`, `PMS7003`, `PMSA003` | `PMS5003S` | `PMS5003T` | `PMS5003ST` | `PMS9003M` |
| PM1.0 STD    | ✅         | ✅         | ✅                                          | ✅          | ✅          | ✅           | ✅          |
| PM2.5 STD    | ✅         | ✅         | ✅                                          | ✅          | ✅          | ✅           | ✅          |
| PM10.0 STD   | ✅         | ✅         | ✅                                          | ✅          | ✅          | ✅           | ✅          |
| PM1.0        | ✅         | ✅         | ✅                                          | ✅          | ✅          | ✅           | ✅          |
| PM2.5        | ✅         | ✅         | ✅                                          | ✅          | ✅          | ✅           | ✅          |
| PM10.0       | ✅         | ✅         | ✅                                          | ✅          | ✅          | ✅           | ✅          |
| PM0.3 LoA    | ✅         | ❌         | ✅                                          | ✅          | ✅          | ✅           | ✅          |
| PM0.5 LoA    | ✅         | ❌         | ✅                                          | ✅          | ✅          | ✅           | ✅          |
| PM1.0 LoA    | ✅         | ❌         | ✅                                          | ✅          | ✅          | ✅           | ✅          |
| PM2.5 LoA    | ✅         | ❌         | ✅                                          | ✅          | ✅          | ✅           | ✅          |
| PM5.0 LoA    | ✅         | ❌         | ✅                                          | ✅          | ❌          | ✅           | ✅          |
| PM10.0 LoA   | ✅         | ❌         | ✅                                          | ✅          | ❌          | ✅           | ✅          |
| Formaldehyde | ❌         | ❌         | ❌                                          | ✅          | ❌          | ✅           | ❌          |
| Temperature  | ❌         | ❌         | ❌                                          | ❌          | ✅          | ✅           | ❌          |
| Humidity     | ❌         | ❌         | ❌                                          | ❌          | ✅          | ✅           | ❌          |
| Version¹     | ✅         | ❌         | ❌                                          | ❌          | ❌          | ✅           | ✅          |
| Error Code¹  | ✅         | ❌         | ❌                                          | ❌          | ❌          | ✅           | ✅          |

¹ Currently not supported/provided as sensor by esphome

## Sensor Longevity

The laser diode inside the PMSX003 has a lifetime of about 8000 hours, nearly one year.

If you wish to use the optional `update_interval` ensure you have a `tx_pin` set in the UART configuration and connected to the RECEIVE/RX pin
(may also be called the TX pin, depending on the model) of the PMS. Setting `update_interval` to 120 seconds or higher may help extend the life span of the sensor.

```yaml
# Example configuration entry
sensor:
  - platform: pmsx003
    type: PMSX003
    pm_1_0:
      name: "Particulate Matter <1.0µm Concentration"
    pm_2_5:
      name: "Particulate Matter <2.5µm Concentration"
    pm_10_0:
      name: "Particulate Matter <10.0µm Concentration"
```

## Configuration variables

- **pm_1_0_std** (*Optional*): Use the concentration of particulates of size less than 1.0µm in µg per cubic meter at standard particle.
  All options from [Sensor](/components/sensor).

- **pm_2_5_std** (*Optional*): Use the concentration of particulates of size less than 2.5µm in µg per cubic meter at standard particle.
  All options from [Sensor](/components/sensor).

- **pm_10_0_std** (*Optional*): Use the concentration of particulates of size less than 10.0µm in µg per cubic meter at standard particle.
  All options from [Sensor](/components/sensor).

- **pm_1_0** (*Optional*): Use the concentration of particulates of size less than 1.0µm in µg per cubic meter under atmospheric environment.
  All options from [Sensor](/components/sensor).

- **pm_2_5** (*Optional*): Use the concentration of particulates of size less than 2.5µm in µg per cubic meter under atmospheric environment.
  All options from [Sensor](/components/sensor).

- **pm_10_0** (*Optional*): Use the concentration of particulates of size less than 10.0µm in µg per cubic meter under atmospheric environment.
  All options from [Sensor](/components/sensor).

- **pm_0_3um** (*Optional*): Use the number of particles with diameter beyond 0.3um in 0.1L of air.
  All options from [Sensor](/components/sensor).

- **pm_0_5um** (*Optional*): Use the number of particles with diameter beyond 0.5um in 0.1L of air.
  All options from [Sensor](/components/sensor).

- **pm_1_0um** (*Optional*): Use the number of particles with diameter beyond 1.0um in 0.1L of air.
  All options from [Sensor](/components/sensor).

- **pm_2_5um** (*Optional*): Use the number of particles with diameter beyond 2.5um in 0.1L of air.
  All options from [Sensor](/components/sensor).

- **pm_5_0um** (*Optional*): Use the number of particles with diameter beyond 5.0um in 0.1L of air. Not supported by the `PMS5003T` type sensors.
  All options from [Sensor](/components/sensor).

- **pm_10_0um** (*Optional*): Use the number of particles with diameter beyond 10.0um in 0.1L of air. Not supported by the `PMS5003T` type sensors.
  All options from [Sensor](/components/sensor).

- **formaldehyde** (*Optional*): Use the formaldehyde (HCHO) concentration in µg per cubic meter for the `PMS5003S` and `PMS5003ST` type sensors.
  All options from [Sensor](/components/sensor).

- **temperature** (*Optional*): Use the temperature value in °C for the `PMS5003T` and `PMS5003ST` type sensors.
  All options from [Sensor](/components/sensor).

- **humidity** (*Optional*): Use the humidity value in % for the `PMS5003T` and `PMS5003ST` type sensors.
  All options from [Sensor](/components/sensor).

- **update_interval** (*Optional*): Amount of time to wait between generating measurements. If this is longer than 30
  seconds, and if `tx_pin` is set in the UART configuration, the fan will be spun down between measurements. Default to `0s` (forward data as it's coming in from the sensor).

- **uart_id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID of the [UART Component](/components/uart) if you want
  to use multiple UART buses.

## See Also

- {{< docref "/components/sensor/aqi" >}}
- {{< docref "/components/sensor/sds011" >}}
- [Sensor Filters](/components/sensor#sensor-filters)
- {{< apiref "pmsx003/pmsx003.h" "pmsx003/pmsx003.h" >}}
