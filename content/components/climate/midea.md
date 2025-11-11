---
description: "Instructions for setting up a Midea climate device"
title: "Midea Air Conditioner"
params:
  seo:
    description: Instructions for setting up a Midea climate device
    image: air-conditioner.svg
---

The `midea` component creates a Midea air conditioner climate device.

> [!NOTE]
> This protocol also used by some vendors:
>
> - [Electrolux](https://www.electrolux.com/en/)
> - [Qlima](https://www.qlima.com/)
> - [Artel](https://www.artelgroup.com/)
> - [Carrier](https://www.carrier.com/)
> - [Comfee](https://www.feelcomfee.com/global/)
> - [Inventor](https://www.inventorairconditioner.com/)
> - [Senville](https://senville.com/)
> - and maybe others
>
> Control is possible with a custom dongle. Example of hardware implementation is [IoT Uni Dongle](https://github.com/dudanov/iot-uni-dongle) or [Midea SLWF-01pro](https://smartlight.me/smart-home-devices/wifi-devices/wifi-dongle-air-conditioners-midea-idea-electrolux-for-home-assistant) ([CloudFree](https://cloudfree.shop/product/ductless-hvac-wi-fi-module/), [Tindie](https://www.tindie.com/products/smartlightme/wifi-dongle-for-air-conditioners-midea-electrolux)).

The Midea air conditioner requires the UART to be configured with `baud_rate: 9600`. The hardware requires **5V logic levels** and does not appear to work with 3.3V logic levels. Use a logic level shifter if building your own dongle.

```yaml
# Example configuration entry
# Main settings
climate:
  - platform: midea
    name: Midea Climate         # Use a unique name.
    period: 1s                  # Optional
    timeout: 2s                 # Optional
    num_attempts: 3             # Optional
    autoconf: true              # Autoconfigure most options.
    beeper: true                # Beep on commands.
    visual:                     # Optional. Example of visual settings override.
      min_temperature: 17 °C    # min: 17
      max_temperature: 30 °C    # max: 30
      temperature_step: 0.5 °C  # min: 0.5
    supported_modes:            # Optional. All capabilities in this section may be detected by autoconf.
      - FAN_ONLY
      - HEAT_COOL
      - COOL
      - HEAT
      - DRY
    custom_fan_modes:           # Optional
      - SILENT
      - TURBO
    supported_presets:          # Optional. All capabilities in this section may be detected by autoconf.
      - ECO
      - BOOST
      - SLEEP
    custom_presets:             # Optional. All capabilities in this section may be detected by autoconf.
      - FREEZE_PROTECTION
    supported_swing_modes:      # Optional
      - VERTICAL
      - HORIZONTAL
      - BOTH
    outdoor_temperature:        # Optional. Outdoor temperature sensor (may display incorrect values after long inactivity).
      name: Temp
    power_usage:                # Optional. Power usage sensor (only for devices that support this feature).
      name: Power
    humidity_setpoint:          # Optional. Indoor humidity sensor (only for devices that support this feature).
      name: Humidity
```

## Configuration variables

- **uart_id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID of the {{< docref "../uart" >}} if you want
  to use multiple UART buses.

- **transmitter_id** (*Optional*, [ID](/guides/configuration-types#id)): Defined and used automatically when using {{< docref "../remote_transmitter" >}} component for IR commands transmit.
- **period** (*Optional*, [Time](/guides/configuration-types#time)): Minimal period between requests to the appliance. Defaults to `1s`.
- **timeout** (*Optional*, [Time](/guides/configuration-types#time)): Request response timeout until next request attempt. Defaults to `2s`.
- **num_attempts** (*Optional*, int): Number of request attempts between 1 and 5 inclusive. Defaults to `3`.
- **autoconf** (*Optional*, boolean): Get capabilities automatically. Allows you not to manually define most of the capabilities of the appliance.
  Defaults to `True`.

- **beeper** (*Optional*, boolean): Beeper feedback on command. Defaults to `False`.
- **supported_modes** (*Optional*, list): List of supported modes. Possible values are: `HEAT_COOL`, `COOL`, `HEAT`, `DRY`, `FAN_ONLY`.
- **custom_fan_modes** (*Optional*, list): List of supported custom fan modes. Possible values are: `SILENT`, `TURBO`.
- **supported_presets** (*Optional*, list): List of supported presets. Possible values are: `ECO`, `BOOST`, `SLEEP`.
- **custom_presets** (*Optional*, list): List of supported custom presets. Possible values are: `FREEZE_PROTECTION`.
- **supported_swing_modes** (*Optional*, list): List of supported swing modes. Possible values are: `VERTICAL`, `HORIZONTAL`, `BOTH`.
- **outdoor_temperature** (*Optional*): The information for the outdoor temperature
  sensor.

  - All options from [Sensor](/components/sensor).
- **power_usage** (*Optional*): The information for the current power consumption
  sensor.

  - All options from [Sensor](/components/sensor).
- **humidity_setpoint** (*Optional*): The information for the humidity indoor
  sensor (experimental).

  - All options from [Sensor](/components/sensor).
- All other options from [Climate](/components/climate#config-climate).

## Automations

{{< anchor "midea_ac-power_on_action" >}}

### `midea_ac.power_on` Action

This action turn on power. The mode and preset will be restored to the last state before turned off.

```yaml
on_...:
  then:
    - midea_ac.power_on:
```

{{< anchor "midea_ac-power_off_action" >}}

### `midea_ac.power_off` Action

This action turn off power.

```yaml
on_...:
  then:
    - midea_ac.power_off:
```

{{< anchor "midea_ac-power_toggle_action" >}}

### `midea_ac.power_toggle` Action

This action toggle the power state. Identical to pressing the power button on the remote control.

```yaml
on_...:
  then:
    - midea_ac.power_toggle:
```

{{< anchor "midea_ac-follow_me_action" >}}

### `midea_ac.follow_me` Action

This action transmits an IR FollowMe command telling the air conditioner a more accurate
room temperature value to be used instead of the internal indoor unit sensor.

```yaml
on_...:
  then:
    - midea_ac.follow_me:
        temperature: !lambda "return x;"
        use_fahrenheit: false
        beeper: false
```

Configuration variables:

- **temperature** (**Required**, float, [templatable](/automations/templates)):
  Sets the value of an internal temperature sensor. The value will be **clamped** to the range:

  - *0 °C to 37 °C* when `use_fahrenheit` is `false`.
  - *32 °F to 99 °F* when `use_fahrenheit` is `true`.

- **use_fahrenheit** (*Optional*, boolean, [templatable](/automations/templates)):
  Specifies if the `temperature` value is in Fahrenheit. When set to `true`, the temperature is parsed and sent in Fahrenheit. Defaults to `false` (Celsius).

- **beeper** (*Optional*, boolean, [templatable](/automations/templates)):
  Sets beep on update. Defaults to `false`.

{{< anchor "midea_ac-display_toggle_action" >}}

### `midea_ac.display_toggle` Action

This action toggle ac screen. Works via UART if supported or {{< docref "../remote_transmitter" >}}.

```yaml
on_...:
  then:
    - midea_ac.display_toggle:
```

{{< anchor "midea_ac-swing_step_action" >}}

### `midea_ac.swing_step` Action

This action adjust the louver by one step. {{< docref "../remote_transmitter" >}} required.

```yaml
on_...:
  then:
    - midea_ac.swing_step:
```

{{< anchor "midea_ac-beeper_on_action" >}}

### `midea_ac.beeper_on` Action

This action turn on beeper feedback.

```yaml
on_...:
  then:
    - midea_ac.beeper_on:
```

{{< anchor "midea_ac-beeper_off_action" >}}

### `midea_ac.beeper_off` Action

This action turn off beeper feedback.

```yaml
on_...:
  then:
    - midea_ac.beeper_off:
```

## Additional control options using IR commands

It is possible to use the FollowMe function and some other features available only through IR commands.
Below is an example of how to send FollowMe commands with the values of your sensor using the {{< docref "../remote_transmitter" >}}
component, as well as control the light of the LED display.

```yaml
# Example configuration entry

remote_transmitter:
  pin: GPIO13                       # For iot-uni-stick.
  carrier_duty_percent: 100%        # 50% for IR LED, 100% for direct connect to TSOP IR receiver output.

sensor:
  - platform: homeassistant
    entity_id: sensor.room_sensor   # Sensor from HASS
    internal: true
    filters:
      - throttle: 10s
      - heartbeat: 2min             # Maximum interval between updates.
      - debounce: 1s
    on_value:
      midea_ac.follow_me:
        temperature: !lambda "return x;"
        beeper: false               # Optional. Beep on update.

# template buttons for sending display control command and swing step actions
button:
  - platform: template
    name: Display Toggle
    icon: mdi:theme-light-dark
    on_press:
      midea_ac.display_toggle:
  - platform: template
    name: Swing Step
    icon: mdi:tailwind
    on_press:
      midea_ac.swing_step:
```

## Example of Beeper Control Using a Switch

```yaml
switch:
  - platform: template
    name: Beeper
    icon: mdi:volume-source
    optimistic: true
    turn_on_action:
      midea_ac.beeper_on:
    turn_off_action:
      midea_ac.beeper_off:
```

## Acknowledgments

Thanks to the following people for their contributions to reverse engineering the UART protocol and source code in the following repositories:

- [Mac Zhou](https://github.com/mac-zhou/midea-msmart)
- [NeoAcheron](https://github.com/NeoAcheron/midea-ac-py)
- [Rene Klootwijk](https://github.com/reneklootwijk/midea-uart)

Special thanks to the project [IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266) for describing the IR protocol.

## See Also

- {{< docref "/components/climate" >}}
- {{< apiref "climate/midea_ac.h" "climate/midea_ac.h" >}}
