---
description: "Instructions for setting up LEDC hardware PWM outputs on the ESP32."
title: "ESP32 LEDC Output"
params:
  seo:
    description: Instructions for setting up LEDC hardware PWM outputs on the ESP32.
    image: pwm.png
---

The LEDC output component exposes a [LEDC PWMchannel](https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/peripherals/ledc.html)
of the ESP32 as an output component.

The frequency range of LEDC is from 10Hz to 40MHz - however, higher frequencies require a smaller
bit depth which means the output is not that accurate for frequencies above ~300kHz.

## Configuration variables

- **pin** (**Required**, [Pin](/guides/configuration-types#pin)): The pin to use LEDC on. Can only be GPIO0-GPIO33.
- **id** (**Required**, [ID](/guides/configuration-types#id)): The id to use for this output component.
- **frequency** (*Optional*, float): At which frequency to run the LEDC
  channel's timer. Defaults to 1000Hz.

- All other options from [Output](/components/output#config-output).

Advanced options:

- **channel** (*Optional*, int): Manually set the [LEDC channel](https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/peripherals/ledc.html#configure-channel)
  to use. Two adjacent channels share the same timer. Defaults to an automatic selection.

Note: When configuring custom frequencies for two or more outputs, ensure that you manually specify
channel 0, 2, 4, 6 for each output. This will prevent issues that arise from automatic selection,
which chooses adjacent channels with shared timers. See
[Issue #3114](https://github.com/esphome/issues/issues/3114) for more details.

- **phase_angle** (*Optional*, float): Set a phase angle to the other channel of this timer.
  Range 0-360°, defaults to 0°

Note: this variable is only available for the esp-idf framework

### Example Usage For a Light

```yaml
# Example configuration entry
output:
  - platform: ledc
    pin: GPIOXX
    id: gpio_

# Example usage in a light
light:
  - platform: monochromatic
    output: gpio_19
    name: "Kitchen Light"
```

### Example Usage For a Piezo Buzzer

```yaml
# Configure the output
output:
  - platform: ledc
    ######################################################
    # One buzzer leg connected to GPIO12, the other to GND
    ######################################################
    pin: GPIO12
    id: buzzer

# Example usage in an automation
on_press:
    then:
    ######################################################
    # Must be turned on before setting frequency & level
    ######################################################
    - output.turn_on: buzzer
    ######################################################
    # Frequency sets the wave size
    ######################################################
    - output.ledc.set_frequency:
        id: buzzer
        frequency: "1000Hz"
    ######################################################
    # level sets the %age time the PWM is on
    ######################################################
    - output.set_level:
        id: buzzer
        level: "50%"
```

## Recommended frequencies

To get the highest available frequency while still getting the same bit depth it is
recommended to pick one of the following frequencies.

Higher bit depth means that the light has more steps available to change from one
value to another. This is especially noticeable when the light is below 10% and takes
a long transition, e.g. turning slowly off.

| **Frequency** | **Bit depth** | **Available steps for transitions** |
| ------------- | ------------- | ----------------------------------- |
| 1220Hz        | 16            | 65536                               |
| 2441Hz        | 15            | 32768                               |
| 4882Hz | 14 | 16384 |
| 9765Hz | 13 | 8192 |
| 19531Hz | 12 | 4096 |

The ESP8266 for instance has *usually* a frequency of 1000Hz with a resolution of 10 bits.
This means that there are only 4 steps between each value.

{{< anchor "output-ledc-set_frequency_action" >}}

## `output.ledc.set_frequency` Action

This [Action](/automations/actions#all-actions) allows you to manually change the frequency of an LEDC
channel at runtime. Use cases include controlling a passive buzzer (for pitch control).

```yaml
on_...:
  - output.ledc.set_frequency:
      id: ledc_output
      frequency: 100Hz
```

Configuration variables:

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the LEDC output to change.
- **frequency** (**Required**, [templatable](/automations/templates), float): The frequency
  to set in hertz.

## See Also

- {{< docref "/components/output" >}}
- {{< docref "/components/output/esp8266_pwm" >}}
- {{< docref "/components/light/monochromatic" >}}
- {{< docref "/components/fan/speed" >}}
- {{< docref "/components/power_supply" >}}
- {{< apiref "ledc/ledc_output.h" "ledc/ledc_output.h" >}}
- [esp-idf LEDC API docs](https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/peripherals/ledc.html)
