---
description: "Instructions for setting up an Air Quality Index (AQI) sensor"
title: "Air Quality Index (AQI)"
params:
  seo:
    description: Instructions for setting up an Air Quality Index (AQI) sensor
---

The `aqi` sensor platform allows you to compute an Air Quality Index from
PM2.5 and PM10 particulate matter sensor readings. This sensor works with
any PM sensor source, such as {{< docref "/components/sensor/pmsx003" >}},
{{< docref "/components/sensor/hm3301" >}}, {{< docref "/components/sensor/sds011" >}},
or {{< docref "/components/sensor/sps30" >}}.

> [!NOTE]
> This platform replaces the deprecated `aqi` option previously available in the
> [HM3301](/components/sensor/hm3301) component. The standalone platform is more
> flexible as it works with any PM sensor.

Two calculation types are supported:

- **AQI**: US EPA Air Quality Index (0-500 scale)
- **CAQI**: European Common Air Quality Index (0-100+ scale)

```yaml
# Example configuration entry
sensor:
  - platform: pmsx003
    type: PMSX003
    pm_2_5:
      id: pm25_sensor
      name: "PM2.5"
    pm_10_0:
      id: pm10_sensor
      name: "PM10"

  - platform: aqi
    name: "Air Quality Index"
    pm_2_5: pm25_sensor
    pm_10_0: pm10_sensor
    calculation_type: AQI
```

## Configuration variables

- **pm_2_5** (**Required**, [ID](/guides/configuration-types#id) of a {{< docref "/components/sensor" >}}):
  The sensor providing PM2.5 concentration readings in µg/m³.

- **pm_10_0** (**Required**, [ID](/guides/configuration-types#id) of a {{< docref "/components/sensor" >}}):
  The sensor providing PM10 concentration readings in µg/m³.

- **calculation_type** (**Required**, enum): The AQI calculation standard to use.
  Must be one of:

  - `AQI`: US EPA Air Quality Index. Returns values from 0-500, where higher values
    indicate worse air quality. Based on the
    [EPA Technical Assistance Document](https://document.airnow.gov/technical-assistance-document-for-the-reporting-of-daily-air-quailty.pdf).

  - `CAQI`: European Common Air Quality Index. Returns values from 0 upward, where
    higher values indicate worse air quality. Typically 0-25 is very low, 25-50 is low,
    50-75 is medium, 75-100 is high, and >100 is very high.

- All other options from [Sensor](/components/sensor).

## Example with CAQI

```yaml
sensor:
  - platform: hm3301
    pm_2_5:
      id: pm25
      name: "PM2.5"
    pm_10_0:
      id: pm10
      name: "PM10"

  - platform: aqi
    name: "European Air Quality Index"
    pm_2_5: pm25
    pm_10_0: pm10
    calculation_type: CAQI
```

## See Also

- {{< docref "/components/sensor/pmsx003" >}}
- {{< docref "/components/sensor/hm3301" >}}
- {{< docref "/components/sensor/sds011" >}}
- {{< docref "/components/sensor/sps30" >}}
- {{< apiref "aqi/aqi_sensor.h" "aqi/aqi_sensor.h" >}}
