---
description: "Instructions for setting up I²S based media players in ESPHome."
title: "I²S Audio Media Player"
params:
  seo:
    description: Instructions for setting up I²S based media players in ESPHome.
    image: i2s_audio.svg
---

The `i2s_audio` media player platform allows you to play media from webservers and web streams
via the {{< docref "/components/i2s_audio" >}}. This platform only works on ESP32 based chips.

```yaml
# Example configuration entry
media_player:
  - platform: i2s_audio
    name: ESPHome I2S Media Player
    dac_type: external
    i2s_dout_pin: GPIOXX
    mode: mono
```

## Configuration variables

- **dac_type** (**Required**, enum):

  - `external`  : Use an external DAC, for example the NS4168, or UDA1334A.
  - `internal`  : Use the internal DAC

- All other options from [Media Player](/components/media_player#config-media_player)

### External DAC

- **i2s_dout_pin** (**Required**, [Pin](/guides/configuration-types#pin)): The GPIO pin to use for the I²S `DOUT/SDOUT` *(Data Out)* signal, also referred to as `SD/SDATA` *(Serial Data)* or `DACDAT` *(Digital to Analog Converter Data)*.
- **mute_pin** (*Optional*, [Pin Schema](/guides/configuration-types#pin-schema)): The GPIO pin to use to mute the media player.
- **mode** (*Optional*, string): The mode of the I²S bus. Can be `mono` or `stereo`. Defaults to `mono`.
- **i2s_audio_id** (*Optional*, [ID](/guides/configuration-types#id)): The ID of the [I²S Audio](/components/i2s_audio#i2s_audio) you wish to use for this media player.
- **i2s_comm_fmt** (*Optional*, string): I2S communication format. By default MSB format is used (AC101, PCM5102A).
  Set to `lsb` if using an external DAC that uses Japanese (Least Significant Bit Justified) format (like PT8211).
  Can be `msb` or `lsb`. Defaults to `msb`.

For best results, keep the wires as short as possible.

### Internal DAC

- **mode** (**Required**, enum): The channel mode of the internal DAC.

  - `left`
  - `right`
  - `stereo`

## See also

- {{< docref "index/" >}}
