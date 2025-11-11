---
description: "Instructions for setting up ESP32 digital-to-analog converter."
title: "ESP32 DAC"
params:
  seo:
    description: Instructions for setting up ESP32 digital-to-analog converter.
    image: dac.svg
---

The ESP32 DAC platform allows you to output analog voltages using the 8-bit digital-to-analog
converter of the ESP32. Unlike the {{< docref "/components/output/ledc" >}}, which can simulate an analog
signal by using a fast switching frequency, the hardware DAC can output a *real* analog signal with
no need for additional filtering.

The DAC spans across two pins, each on its own channel:

- ESP32: GPIO25 (Channel 0) and GPIO26 (Channel 1).
- ESP32 S2: GPIO17 (Channel 0) and GPIO18 (Channel 1).

The output level is a percentage of the board supply voltage (VDD_A) - generally this will be 3.3 V.

```yaml
# Example configuration entry
output:
  - platform: esp32_dac
    pin: GPIO25
    id: dac_output

# Example usage
on_...:
  then:
    - output.set_level:
        id: dac_output
        level: 50%

# Use the DAC output as a light
light:
  - platform: monochromatic
    output: dac_output
    gamma_correct: 1.4
    id: mono_light
```

## Configuration variables

- **pin** (**Required**, [Pin Schema](/guides/configuration-types#pin-schema)): The pin to use DAC on. See above for valid pin numbers.
- **id** (**Required**, [ID](/guides/configuration-types#id)): The id to use for this output component.
- All other options from [Output](/components/output#config-output).

## Use Cases

- Generating a specific (and dynamic) reference voltage for an external sensor or ADC, such as the
  {{< docref "/components/sensor/ads1115" >}}

- Controlling the bias of a transistor
- Driving a bar graph or large amount of LEDs using an analog-controlled LED driver like the LM3914
  ([datasheet](https://www.ti.com/lit/ds/symlink/lm3914.pdf)); this can allow you to make tank
  level indicators, temperature gauges, and so on from a single output pin

- Generating 0-10 V for a dimmable light (operational amplifier required)

## See Also

- {{< docref "/components/output" >}}
- {{< docref "/components/output/ledc" >}}
- {{< docref "/components/output/esp8266_pwm" >}}
- {{< docref "/components/light/monochromatic" >}}
- {{< docref "/components/fan/speed" >}}
- {{< docref "/components/power_supply" >}}
- {{< apiref "esp32_dac/esp32_dac.h" "esp32_dac/esp32_dac.h" >}}
