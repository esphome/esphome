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
    aqi:
      name: "Air Quality Index"
      calculation_type: "AQI"
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

- **aqi** (*Optional*): Air Quality Index sensor. Requires both `pm_2_5` and `pm_10_0`
  sensors to be configured. See [Air Quality Index](#air-quality-index) below.

  - **calculation_type** (**Required**): The AQI calculation standard to use.
    One of: `AQI` (US EPA) or `CAQI` (European).
  - All other options from [Sensor](/components/sensor).

- **update_interval** (*Optional*): Amount of time to wait between generating measurements. If this is longer than 30
  seconds, and if `tx_pin` is set in the UART configuration, the fan will be spun down between measurements. Default to `0s` (forward data as it's coming in from the sensor).

- **uart_id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID of the [UART Component](/components/uart) if you want
  to use multiple UART buses.

## Air Quality Index

The AQI (Air Quality Index) sensor calculates an air quality index value based on the
PM2.5 and PM10 particulate matter concentrations. This provides a single number that
indicates overall air quality and associated health concerns.

Two calculation standards are supported:

- **AQI** (US EPA Air Quality Index): The standard used in the United States, Canada,
  and parts of Asia. Scale of 0-500+.
- **CAQI** (Common Air Quality Index): The European standard. Scale of 0-400.

Both calculation types take the PM2.5 and PM10 values and return the higher
(more conservative) of the two calculated index values.

### AQI Scale (US EPA)

| Index | Level | Health Implications |
|-------|-------|---------------------|
| 0-50 | Good | Air quality is satisfactory |
| 51-100 | Moderate | Some pollutants may be a concern for sensitive individuals |
| 101-150 | Unhealthy for Sensitive Groups | Sensitive groups may experience health effects |
| 151-200 | Unhealthy | Everyone may begin to experience health effects |
| 201-300 | Very Unhealthy | Everyone may experience more serious health effects |
| 301-500 | Hazardous | Health warnings of emergency conditions |

### CAQI Scale (European)

| Index | Level | Health Implications |
|-------|-------|---------------------|
| 0-25 | Very Low | Air quality is excellent |
| 26-50 | Low | Air quality is good |
| 51-75 | Medium | Air quality is fair |
| 76-100 | High | Air quality is poor |
| 101-400 | Very High | Air quality is very poor |

### Configuration Example

```yaml
sensor:
  - platform: pmsx003
    type: PMSX003
    pm_2_5:
      name: "PM2.5"
    pm_10_0:
      name: "PM10"
    aqi:
      name: "Air Quality Index"
      calculation_type: "AQI"  # or "CAQI" for European standard
      # Optional: Apply filters for smoother values
      filters:
        - sliding_window_moving_average:
            window_size: 15
            send_every: 1
```

## See Also

- {{< docref "/components/sensor/sds011" >}}
- [Sensor Filters](/components/sensor#sensor-filters)
- {{< apiref "pmsx003/pmsx003.h" "pmsx003/pmsx003.h" >}}
