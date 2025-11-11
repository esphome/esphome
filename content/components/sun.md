---
description: "Instructions for setting up tracking the sun position in ESPHome."
title: "Sun"
params:
  seo:
    description: Instructions for setting up tracking the sun position in ESPHome.
    image: weather-sunny.svg
---

The `sun` component allows you to track the sun's position in the sky. Calculations are done every 60 seconds.

## Component/Hub

```yaml
# Example configuration entry
sun:
  latitude: 48.8584°
  longitude: 2.2945°

# At least one time source is required
time:
  - platform: homeassistant
```

### Configuration variables

- **latitude** (**Required**, float): The latitude for performing the calculation.
- **longitude** (**Required**, float): The longitude for performing the calculation.
- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.

## Triggers

```yaml
# Example configuration entry
sun:
  latitude: 48.8584°
  longitude: 2.2945°

  on_sunrise:
    - then:
        - logger.log: Good morning!
    # Custom elevation, will be called shortly after the trigger above.
    - elevation: 5°
      then:
        - logger.log: Good morning 2!

  on_sunset:
    - then:
        - logger.log: Good evening!
```

- **on_sunrise** (*Optional*, [Automation](/automations)): An automation to perform at sunrise
  when the sun crosses a specified angle.

  - **elevation** (*Optional*, float): The elevation to cross. Defaults to -0.833° (the horizon, slightly less than 0°
    to compensate for atmospheric refraction).

- **on_sunset** (*Optional*, [Automation](/automations)): An automation to perform at sunset
  when the sun crosses a specified angle.

  - **elevation** (*Optional*, float): The elevation to cross. Defaults to -0.833° (the horizon, slightly less than 0°
    to compensate for atmospheric refraction).

## Sensor

Additionally, the sun component exposes its values over a sensor platform.

```yaml
# Example configuration entry
sensor:
  - platform: sun
    name: Sun Elevation
    type: elevation
  - platform: sun
    name: Sun Azimuth
    type: azimuth
```

{{< img src="sun-sensor-ui.png" alt="Image" width="80.0%" class="align-center" >}}

### Configuration variables

- **type** (**Required**, string): The type of value to track. One of `elevation` and
  `azimuth`.

- All other options from [Sensor](/components/sensor).

## Text Sensor

Other properties like the next sunset time can be read out with the sun text_sensor platform.

```yaml
# Example configuration entry
text_sensor:
  - platform: sun
    name: Sun Next Sunrise
    type: sunrise
  - platform: sun
    name: Sun Next Sunset
    type: sunset
```

{{< img src="sun-text_sensor-ui.png" alt="Image" width="80.0%" class="align-center" >}}

### Configuration variables

- **type** (**Required**, string): The type of value to track. One of `sunrise` and
  `sunset`.

- **elevation** (*Optional*, float): The elevation to calculate the next sunrise/sunset event
  for. Defaults to -0.833° (the horizon, slightly less than 0° to compensate for atmospheric refraction).

- **format** (*Optional*, string): The format to format the time value with, see [strftime](/components/time#strftime)
  for more information. Defaults to `%X`.

- All other options from [Text Sensor](/components/text_sensor#config-text_sensor).

{{< anchor "sun-is_above_below_horizon-condition" >}}

## `sun.is_above_horizon` / `sun.is_below_horizon` Conditions

The `sun.is_above_horizon` and `sun.is_below_horizon` [conditions](/automations/actions#all-conditions)
allow you to check if the sun is currently above or below the horizon.

```yaml
on_...:
  - if:
      condition:
        - sun.is_above_horizon:
      then:
        - logger.log: Sun is above horizon!
```

## See Also

- {{< apiref "sun/sun.h" "sun/sun.h" >}}
