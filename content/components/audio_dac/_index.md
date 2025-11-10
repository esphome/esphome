---
description: "Instructions for using ESPHome's Audio DAC Core component."
title: "Audio DAC Core"
params:
  seo:
    description: Instructions for using ESPHome's Audio DAC Core component.
    image: i2s_audio.svg
---

The `audio_dac` component allows your ESPHome devices to use audio DAC hardware components, allowing the playback of
audio via the microcontroller from a range of sources via {{< docref "/components/media_player" >}}.

```yaml
# Example configuration entry
audio_dac:
  - platform: ...
```

## Platforms

{{< anchor "config-audio_dac" >}}

## Configuration variables

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.

{{< anchor "automations-audio_dac" >}}

## Automations

### `audio_dac.mute_off` Action

This action unmutes the output of the DAC.

Configuration variables:

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the `audio_dac` platform.

### `audio_dac.mute_on` Action

This action mutes the output of the DAC.

Configuration variables:

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the `audio_dac` platform.

### `audio_dac.set_volume` Action

This action sets the output volume of the DAC.

Configuration variables:

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the `audio_dac` platform.
- **volume** (**Required**, percentage, [templatable](/automations/templates)): The desired volume level for the
  output from 0% to 100%.

## See Also
