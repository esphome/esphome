---
description: "Instructions for setting up I²S based speakers in ESPHome."
title: "I²S Audio Speaker"
params:
  seo:
    description: Instructions for setting up I²S based speakers in ESPHome.
    image: i2s_audio.svg
---

The `i2s_audio` speaker platform allows you to receive audio via the {{< docref "/components/i2s_audio" >}}.

This platform only works on ESP32 based chips.

> [!WARNING]
> Audio and voice components consume a significant amount of resources (RAM, CPU) on the device.
>
> **Crashes are likely to occur** if you include too many additional components in your device's
> configuration. In particular, Bluetooth/BLE components are known to cause issues when used in
> combination with Voice Assistant and/or other audio components.

```yaml
# Example configuration entry
speaker:
  - platform: i2s_audio
    dac_type: external
    i2s_dout_pin: GPIOXX
```

## Configuration variables

- **dac_type** (**Required**, enum):

  - `external`  : Use an external DAC, for example the NS4168, or UDA1334A.
  - `internal`  : Use the internal DAC

- **channel** (*Optional*, enum): The channel of the speaker. One of `left`, `right`, `mono`, or `stereo`. If `stereo`, the input data should be twice as big,
  with each right sample followed by a left sample. `left` and `right` mute the unused channel, while `mono` plays the same samples on both. Defaults to `mono`.

- **sample_rate** (*Optional*, positive integer): I2S sample rate. If in `primary` I²S mode the sample rate of the audio stream is used. Defaults to `16000`.
- **bits_per_sample** (*Optional*, enum): The bit depth of the audio samples sent to the DAC. One of `8bit`, `16bit`, `24bit`, or `32bit`. Defaults to `16bit`.
- **bits_per_channel** (*Optional*, enum): The bit depth of the audio channels. This is what is actually send to the DAC and needs to be equal or larger than `bits_per_sample`.
  See the datasheet of your I2S device for details. Defaults to `bits_per_sample`. Setting is ignored if the legacy driver is not used.

- **mclk_multiple** (*Optional*, enum): The multiple of the MCLK frequency to the sample rate. Must be divisible by 3 if using 24 bits per sample. One of `128`, `256`, `384`, `512`. Defaults to `256`.
- **use_apll** (*Optional*, boolean): I2S using APLL as main I2S clock, enable it to get accurate clock. Defaults to `false`.
- **i2s_mode** (*Optional*, enum): The I²S mode to use. One of `primary` (clock driven by the host) or `secondary` (clock driven by the attached device). Defaults to `primary`.
- **i2s_audio_id** (*Optional*, [ID](/guides/configuration-types#id)): The ID of the [I²S Audio](/components/i2s_audio#i2s_audio) you wish to use for this speaker.
- **i2s_comm_fmt** (*Optional*, enum): The I²S communication standard format used by this speaker.

  - `stand_i2s` (Default)
  - `stand_msb`
  - `stand_pcm_short`
  - `stand_pcm_long`
  - `stand_max` (only with legacy driver)
  - `i2s_msb`
  - `i2s_lsb`
  - `pcm`
  - `pcm_short`
  - `pcm_long`
- **buffer_duration** (*Optional*, [Time](/guides/configuration-types#time)): The duration of the internal ring buffer. Larger values can reduce stuttering but uses more memory. Defaults to `500ms`.
- **timeout** (*Optional*, [Time](/guides/configuration-types#time)): How long to wait after finishing playback before releasing the bus. Set to `never` to never stop the speaker due to a timeout. Defaults to `500ms`.
- All other options from [Speaker Component](/components/speaker#config-speaker).

### External DAC

- **i2s_dout_pin** (**Required**, [Pin Schema](/guides/configuration-types#pin-schema)): The GPIO pin to use for the I²S DOUT (Data Out) signal.

For best results, keep the wires as short as possible.

### Internal DAC

> [!NOTE]
> Internal DAC speakers are only supported by the legacy I²S driver on a regular ESP32, not the variants.

- **mode** (**Required**, enum): The channel mode of the internal DAC.

  - `left`
  - `right`
  - `stereo`

## See also

- {{< docref "index/" >}}
