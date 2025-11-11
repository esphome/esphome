---
description: "Instructions for setting up PMSX003 Particulate matter sensors"
title: "PMSX003 Particulate Matter Sensor"
params:
  seo:
    description: Instructions for setting up PMSX003 Particulate matter sensors
    image: pmsx003.svg
---

The `pmsx003` sensor platform allows you to use your Plantower PMS5003, PMS7003, ... laser based particulate matter sensors
([datasheet](http://www.aqmd.gov/docs/default-source/aq-spec/resources-page/plantower-pms5003-manual_v2-3.pdf))
sensors with ESPHome.

As the communication with the PMSX003 is done using UART, you need
to have an [UART bus](/components/uart) in your configuration with the `rx_pin` connected to the SEND/TX pin
(may also be called the RX pin, depending on the model) of the PMS. Additionally, you need to set the baud rate to 9600.

This platform supports three sensor types, which you need to specify using the `type:` configuration
value:

- `PMSX003` for generic PMS5003, PMS7003, ...; these sensors support `pm_1_0`, `pm_2_5` and `pm_10_0` output.
- `PMS5003S` for PMS5003S. These support `pm_1_0`, `pm_2_5` and `pm_10_0` and `formaldehyde`.
- `PMS5003T` for PMS5003T. These support `pm_1_0`, `pm_2_5` and `pm_10_0`, `temperature` and `humidity`.
- `PMS5003ST` for PMS5003ST. These support `pm_2_5`, `temperature`, `humidity` and `formaldehyde`.

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

- **temperature** (*Optional*): Use the temperature value in °C for the `PMS5003T` and `PMS5003ST` type sensors.
  All options from [Sensor](/components/sensor).

- **humidity** (*Optional*): Use the humidity value in % for the `PMS5003T` and `PMS5003ST` type sensors.
  All options from [Sensor](/components/sensor).

- **formaldehyde** (*Optional*): Use the formaldehyde (HCHO) concentration in µg per cubic meter for the `PMS5003S` and `PMS5003ST` type sensors.
  All options from [Sensor](/components/sensor).

- **update_interval** (*Optional*): Amount of time to wait between generating measurements. If this is longer than 30
  seconds, and if `tx_pin` is set in the UART configuration, the fan will be spun down between measurements. Default to `0s` (forward data as it's coming in from the sensor).

- **uart_id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID of the [UART Component](/components/uart) if you want
  to use multiple UART buses.

## See Also

- {{< docref "/components/sensor/sds011" >}}
- [Sensor Filters](/components/sensor#sensor-filters)
- {{< apiref "pmsx003/pmsx003.h" "pmsx003/pmsx003.h" >}}
