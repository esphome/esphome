---
description: "Instructions for setting up absolute humidity calculation"
title: "Absolute Humidity"
params:
  seo:
    description: Instructions for setting up absolute humidity calculation
    image: water-drop.svg
---

The `absolute_humidity` platform allows you to calculate absolute humidity from air temperature and relative humidity.

See the links at the bottom of the page for details on absolute humidity and the different saturated vapor pressure equations.

```yaml
# Example configuration entry
sensor:
  - platform: absolute_humidity
    name: Absolute Humidity
    temperature: air_temperature
    humidity: relative_humidity

  # Use any temperature and relative humidity source, e.g. a BME280:
  - platform: ...
    temperature:
      name: Temperature
      id: air_temperature
    humidity:
      name: Relative Humidity
      id: relative_humidity
```

## Configuration variables

- **temperature** (**Required**, [ID](/guides/configuration-types#id)): The sensor that is used to measure the current temperature, in °C.
- **humidity** (**Required**, [ID](/guides/configuration-types#id)): The sensor that is used to measure the current relative humidity, in %.
- **equation** (*Optional*): The saturated vapor pressure equation to use (see below).
- All other options from [Sensor](/components/sensor).

## Saturated vapor pressure equations

There are several different equations for calculating saturated vapor pressure.
`Wobus` is used by default, as it is notionally the most accurate, but any of the following can be used:

- `Buck`  : [Arden Buck equation](https://en.wikipedia.org/wiki/Arden_Buck_equation)
- `Tetens`  : [Tetens equation](https://en.wikipedia.org/wiki/Tetens_equation)
- `Wobus`  : [Wobus equation](https://wahiduddin.net/calc/density_altitude.htm)

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- {{< apiref "absolute_humidity/absolute_humidity.h" "absolute_humidity/absolute_humidity.h" >}}
- [NOAA Discussion on Humidity](https://www.weather.gov/lmk/humidity)
- [Absolute humidity on Wikipedia](https://en.wikipedia.org/wiki/Humidity#Absolute_humidity)
- [How to calculate absolute humidity](https://www.environmentalbiophysics.org/chalk-talk-how-to-calculate-absolute-humidity/)
- [How to convert relative humidity to absolute humidity](https://carnotcycle.wordpress.com/2012/08/04/how-to-convert-relative-humidity-to-absolute-humidity/)
