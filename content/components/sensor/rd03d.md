---
description: "Instructions for setting up RD-03D mmWave radar sensors."
title: "RD-03D mmWave Radar"
params:
  seo:
    description: Instructions for setting up RD-03D mmWave radar sensors.
---

## Component

{{< anchor "rd03d-component" >}}

The `rd03d` component allows you to use the Ai-Thinker RD-03D 24GHz millimeter-wave radar module with ESPHome for
multi-target trajectory tracking.

The RD-03D can simultaneously track up to 3 moving targets, providing real-time position (X/Y coordinates), speed,
and distance information for each target. It is primarily intended for indoor use to track moving human targets.

As an FMCW radar, the RD-03D depends on Doppler shift in order to detect objects and cannot detect or range objects
that are not moving.

The [UART](/components/uart) is required to be set up in your configuration for this sensor to work. The `parity`
must be `NONE`, `stop_bits` must be `1`, and `baud_rate` must be `256000`. Use of a hardware UART is highly
recommended to properly support the 256000 baud rate.

```yaml
# RD-03D configuration
uart:
  rx_pin: GPIOXX
  tx_pin: GPIOXX  # Required only if using tracking_mode
  baud_rate: 256000
  parity: NONE
  stop_bits: 1

rd03d:
  id: rd03d_radar
```

### Configuration variables

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID for this component.
- **uart_id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID of the [UART Component](/components/uart) to use.
  Required if you have multiple UARTs configured.
- **tracking_mode** (*Optional*, string): The tracking mode to configure. If not specified, no command is sent
  and the radar uses its default mode (typically multi-target). Requires a TX pin to be configured.

  - ``single``: Single target tracking mode. The radar will only report the primary detected target.
  - ``multi``: Multi-target tracking mode. The radar can track up to 3 targets simultaneously.

- **throttle** (*Optional*, [Time](/guides/configuration-types#time)): Minimum time between sensor updates.
  The radar sends data very frequently; use this to reduce the update rate sent to Home Assistant.
  For example, ``500ms`` or ``1s``.

{{< anchor "rd03d-binary-sensors" >}}

## Binary Sensor

The `rd03d` binary sensor provides presence detection for targets.

```yaml
binary_sensor:
  - platform: rd03d
    rd03d_id: rd03d_radar
    target:
      name: Presence
    target_1:
      name: Target-1 Presence
    target_2:
      name: Target-2 Presence
    target_3:
      name: Target-3 Presence
```

### Configuration variables

- **rd03d_id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID of the RD-03D component.
- **target** (*Optional*): True if any target is detected. All options from [Binary Sensor](/components/binary_sensor#config-binary_sensor).
- **target_N** (*Optional*): True if the specific target (N = 1 to 3) is detected: `target_1`, `target_2`, `target_3`.
  All options from [Binary Sensor](/components/binary_sensor#config-binary_sensor).

{{< anchor "rd03d-sensors" >}}

## Sensor

The `rd03d` sensor provides detailed information about detected targets.

```yaml
sensor:
  - platform: rd03d
    rd03d_id: rd03d_radar
    target_count:
      name: Target Count
    target_1:
      x:
        name: Target-1 X
      y:
        name: Target-1 Y
      speed:
        name: Target-1 Speed
      angle:
        name: Target-1 Angle
      distance:
        name: Target-1 Distance
      resolution:
        name: Target-1 Resolution
    target_2:
      x:
        name: Target-2 X
      y:
        name: Target-2 Y
      speed:
        name: Target-2 Speed
      angle:
        name: Target-2 Angle
      distance:
        name: Target-2 Distance
      resolution:
        name: Target-2 Resolution
    target_3:
      x:
        name: Target-3 X
      y:
        name: Target-3 Y
      speed:
        name: Target-3 Speed
      angle:
        name: Target-3 Angle
      distance:
        name: Target-3 Distance
      resolution:
        name: Target-3 Resolution
```

### Configuration variables

- **rd03d_id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID of the RD-03D component.
- **target_count** (*Optional*, int): Total number of targets currently detected (0 to 3).
  All options from [Sensor](/components/sensor).

- **target_N** (*Optional*): Details about each target (N = 1 to 3). Up to 3 targets can be tracked simultaneously:
  `target_1`, `target_2`, `target_3`.

  - **x** (*Optional*, int): X coordinate of the target in millimeters. Negative values indicate the left side of the
      sensor, positive values indicate the right side. All options from [Sensor](/components/sensor).

  - **y** (*Optional*, int): Y coordinate of the target in millimeters, representing distance in front of the sensor.
      All options from [Sensor](/components/sensor).

  - **speed** (*Optional*, int): Speed of the target in mm/s. Positive values indicate the target is moving away from
      the sensor, negative values indicate the target is approaching. All options from [Sensor](/components/sensor).

  - **distance** (*Optional*, float): Calculated distance from the sensor to the target in millimeters, derived from
      X and Y coordinates. All options from [Sensor](/components/sensor).

  - **angle** (*Optional*, float): Angle of the target in degrees relative to the sensor's forward direction (Y-axis).
      Negative angles indicate targets to the left, positive angles to the right.
      All options from [Sensor](/components/sensor).

  - **resolution** (*Optional*, int): Target detection resolution in millimeters.
      All options from [Sensor](/components/sensor).

## Example configuration

Here is a complete example configuration for the RD-03D radar.

```yaml
uart:
  id: uart_rd03d
  rx_pin: GPIOXX
  baud_rate: 256000
  parity: NONE
  stop_bits: 1

rd03d:
  id: rd03d_radar
  uart_id: uart_rd03d
  throttle: 1s

binary_sensor:
  - platform: rd03d
    rd03d_id: rd03d_radar
    target:
      name: Presence
    target_1:
      name: Target-1 Presence
    target_2:
      name: Target-2 Presence
    target_3:
      name: Target-3 Presence

sensor:
  - platform: rd03d
    rd03d_id: rd03d_radar
    target_count:
      name: Target Count
    target_1:
      x:
        name: Target-1 X
      y:
        name: Target-1 Y
      speed:
        name: Target-1 Speed
      angle:
        name: Target-1 Angle
      distance:
        name: Target-1 Distance
      resolution:
        name: Target-1 Resolution
    target_2:
      x:
        name: Target-2 X
      y:
        name: Target-2 Y
      speed:
        name: Target-2 Speed
      angle:
        name: Target-2 Angle
      distance:
        name: Target-2 Distance
      resolution:
        name: Target-2 Resolution
    target_3:
      x:
        name: Target-3 X
      y:
        name: Target-3 Y
      speed:
        name: Target-3 Speed
      angle:
        name: Target-3 Angle
      distance:
        name: Target-3 Distance
      resolution:
        name: Target-3 Resolution
```

## See Also

- [RD-03D Datasheet](https://en.ai-thinker.com/Uploads/file/20231016/20231016032622_13559.pdf)
- {{< docref "/components/sensor/ld2410" >}}
- {{< docref "/components/sensor/ld2420" >}}
- {{< docref "/components/sensor/ld2450" >}}
- {{< apiref "rd03d/rd03d.h" "rd03d/rd03d.h" >}}
