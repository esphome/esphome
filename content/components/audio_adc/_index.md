---
description: "Instructions for using ESPHome's Audio ADC Core component."
title: "Audio ADC Core"
params:
  seo:
    description: Instructions for using ESPHome's Audio ADC Core component.
    image: i2s_audio.svg
---

The `audio_adc` component allows your ESPHome devices to use audio ADC hardware components, allowing the
capture/recording of audio via the microcontroller from a range of sources.

```yaml
# Example configuration entry
audio_adc:
  - platform: ...
```

## Platforms

{{< anchor "config-audio_adc" >}}

## Configuration variables

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.

{{< anchor "automations-audio_adc" >}}

## Automations

### `audio_adc.set_mic_gain` Action

This action sets the (microphone) gain of the ADC.

Configuration variables:

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the `audio_adc` platform.
- **mic_gain** (**Required**, percentage, [templatable](/automations/templates)): The desired gain level in decibels
  for the input.

## See Also
