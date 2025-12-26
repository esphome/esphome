---
description: "Instructions for setting up current-based covers in ESPHome."
title: "Current Based Cover"
params:
  seo:
    description: Instructions for setting up current-based covers in ESPHome.
    image: window-open.svg
---

The `current_based` cover platform allows you to create covers with position control by using current
sensors to detect the fully-open and fully-closed states. This is pretty useful when using motors with
integrated mechanical endstops. During cover operation, the component monitors the current consumption
to detect when the motor has stopped.

When fully open or close is requested, the corresponding relay will stay on until the current the motor is
consuming goes below a certain amount. The amount of current needs to be specified in the configuration.

Open and close durations can be specified to allow ESPHome to approximate the current position of the cover.

{{< img src="more-info-ui.png" alt="Image" width="75.0%" class="align-center" >}}

This type of cover also provides safety features like current-based obstacle detection with automatic configurable
rollback as well as relay malfunction detection: operation cancels if there's a current flowing in the opposite
operation circuit (typically caused by welded relays).

> [!WARNING]
> Depending on the cover and motor type, obstacles can physically damage the cover before being detectable.
> Verify your setup to ensure the current consumption will increase enough to be detectable before causing
> any physical damage. Use it at your own risk.

```yaml
# Example configuration entry
cover:
  - platform: current_based
    name: "Current Based Cover"

    open_sensor: open_current_sensor
    open_moving_current_threshold: 0.5
    open_obstacle_current_threshold: 0.8
    open_duration: 12s
    open_action:
      - switch.turn_on: open_cover_switch

    close_sensor: close_current_sensor
    close_moving_current_threshold: 0.5
    close_obstacle_current_threshold: 0.8
    close_duration: 10s
    close_action:
      - switch.turn_on: close_cover_switch

    stop_action:
      - switch.turn_off: close_cover_switch
      - switch.turn_off: open_cover_switch

    obstacle_rollback: 30%
    start_sensing_delay: 0.8s
```

## Configuration variables

- **open_sensor** (**Required**, [ID](/guides/configuration-types#id)): The open current sensor.
- **open_action** (**Required**, [Action](/automations/actions#all-actions)): The action that should
  be performed when the remote requests the cover to be opened.

- **open_duration** (**Required**, [Time](/guides/configuration-types#time)): The amount of time it takes the cover
  to open up from the fully-closed state.

- **open_moving_current_threshold** (**Required**, float): The amount of current in Amps the motor
  should drain to consider the cover is opening.

- **open_obstacle_current_threshold** (**Required**, float): The amount of current in Amps the motor
  should drain to consider the cover is blocked during opening.

- **close_sensor** (**Required**, [ID](/guides/configuration-types#id)): The close current sensor.
- **close_action** (*Optional*, [Action](/automations/actions#all-actions)): The action that should
  be performed when the remote requests the cover to be closed.

- **close_duration** (**Required**, [Time](/guides/configuration-types#time)): The amount of time it takes the cover
  to close from the fully-open state.

- **close_moving_current_threshold** (**Required**, float): The amount of current in Amps the motor
  should drain to consider the cover is closing.

- **close_obstacle_current_threshold** (**Required**, float): The amount of current in Amps the motor
  should drain to consider the cover is blocked during closing.

- **stop_action** (**Required**, [Action](/automations/actions#all-actions)): The action that should
  be performed to stop the cover.

- **max_duration** (*Optional*, [Time](/guides/configuration-types#time)): The maximum duration the cover should be opening
  or closing. Useful for protecting from dysfunctional motor integrated endstops.

- **start_sensing_delay** (*Optional*, [Time](/guides/configuration-types#time)): The amount of time the current sensing will be
  disabled when the movement starts. Motors can take some time before reaching their average consumption.
  Low values can cause an immediate stop because of the first current reading happening in the current-rising period.
  Defaults to `500ms`.

- **obstacle_rollback** (*Optional*, percentage): The percentage of rollback the cover will perform in case of
  obstacle detection. Defaults to `10%`.

- **malfunction_detection** (*Optional*, boolean): Enable to detect malfunction detection (Typically welded relays). Defaults to `True`.
- **malfunction_action** (*Optional*, [Action](/automations/actions#all-actions)): The action that should
  be performed when relay malfunction is detected. Malfunction may require device servicing. You can use this action
  to notify other systems about this situation

- All other options from [Cover](/components/cover#config-cover).

## Use with Shelly 2.5

The Shelly 2.5 is the perfect hardware for this platform. It features two outputs with current monitoring
(thanks to an embedded {{< docref "/components/sensor/ade7953" "ADE7953" >}}) in a very small form factor (39mm x 36mm x 17 mm).
It also features an {{< docref "/components/sensor/ntc" "NTC temperature sensor" >}}.

{{< img src="shelly2.5.png" alt="Image" width="30.0%" class="align-center" >}}

These devices typically run hot (~55Cº at 20ºC room temperature). Long-term heavy loads (near to its rated limit) can overheat the device.
It is strongly recommended to monitor the device temperature using the NTC temperature sensor, shutting down the device if it exceeds 90ºC.
This safety feature is also present in the original firmware.

> [!WARNING]
> The ADE7953 IRQ line is connected to the GPIO16. The `irq_pin` parameter for the {{< docref "/components/sensor/ade7953" "ADE7953" >}} MUST be
> set to GPIO16 to prevent device overheat (>70ºC idling).

Configuration example:

```yaml
esphome:
  name: Shelly 2.5

esp8266:
  board: esp01_1m
  restore_from_flash: true

i2c:
  sda: GPIO12
  scl: GPIO14

sensor:
  - platform: ade7953_i2c
    irq_pin: GPIO16
    voltage:
      name: Shelly 2.5 Mains Voltage
      internal: true
      filters:
        - throttle: 5s
    current_a:
      name: Shelly 2.5 Open Current
      id: open_current
      internal: true
    current_b:
      name: Shelly 2.5 Close Current
      id: close_current
      internal: true
    update_interval: 0.5s

  # NTC Temperature
  - platform: ntc
    sensor: temp_resistance_reading
    name: Shelly 2.5 Temperature
    unit_of_measurement: "°C"
    accuracy_decimals: 1
    calibration:
      b_constant: 3350
      reference_resistance: 10kOhm
      reference_temperature: 298.15K
    on_value_range:
      above: 90
      then: # Security shutdown by overheating
        - switch.turn_on: _shutdown

  - platform: resistance
    id: temp_resistance_reading
    sensor: temp_analog_reading
    configuration: DOWNSTREAM
    resistor: 32kOhm
    internal: true
  - platform: adc
    id: temp_analog_reading
    pin: A0
    update_interval: 30s
    internal: true

binary_sensor:
  - platform: gpio
    pin:
      number: GPIO13
    name: Shelly 2.5 Open Button
    on_press:
      then:
        cover.open: blind

  - platform: gpio
    pin:
      number: GPIO5
    name: Shelly 2.5 Close Button
    on_press:
      then:
        cover.close: blind

switch:
  - platform: shutdown
    id: _shutdown
    name: Shelly 2.5 Shutdown

  - platform: gpio
    id: open_relay
    name: Shelly 2.5 Open Relay
    pin: GPIO15
    restore_mode: RESTORE_DEFAULT_OFF
    interlock: &interlock [open_relay, close_relay]
    interlock_wait_time: 200ms

  - platform: gpio
    id: close_relay
    name: Shelly 2.5 Close Relay
    pin: GPIO4
    restore_mode: RESTORE_DEFAULT_OFF
    interlock: *interlock
    interlock_wait_time: 200ms

# Example configuration entry
cover:
  - platform: current_based
    name: Blind
    id: blind

    open_sensor: open_current
    open_moving_current_threshold: 0.5
    open_duration: 12s
    open_action:
      - switch.turn_on: open_relay
    close_sensor: close_current
    close_moving_current_threshold: 0.5
    close_duration: 10s
    close_action:
      - switch.turn_on: close_relay
    stop_action:
      - switch.turn_off: close_relay
      - switch.turn_off: open_relay
    obstacle_rollback: 30%
    start_sensing_delay: 0.8s
    malfunction_detection: true
    malfunction_action:
      then:
        - logger.log: "Malfunction detected. Relay welded."

status_led:
  pin:
    number: GPIO0
    inverted: yes
```

## See Also

- {{< docref "index/" >}}
- {{< docref "/components/cover/template" >}}
- {{< docref "/components/sensor/ade7953" >}}
- [Automation](/automations)
- {{< apiref "current_based/current_based_cover.h" "current_based/current_based_cover.h" >}}
