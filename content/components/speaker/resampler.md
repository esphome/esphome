---
description: "Instructions for setting up resampler speakers in ESPHome."
title: "Resampler Speaker"
params:
  seo:
    description: Instructions for setting up resampler speakers in ESPHome.
    image: waveform.svg
---

The `resampler` speaker platform allows you to convert the sample rate of an audio stream and output it to another {{< docref "/components/speaker/index" "speaker component" >}}.

If the audio stream doesn't require resampling, it is automatically sent directly to the output speaker.

This platform only works on ESP32 based chips.

> [!WARNING]
> Audio and voice components consume a significant amount of resources (RAM and CPU) on the device.
>
> **Crashes are likely to occur** if you include too many additional components in your device's
> configuration. In particular, Bluetooth/BLE components are known to cause issues when used in
> combination with Voice Assistant and/or other audio components.

```yaml
# Example configuration entry
speaker:
  - platform: resampler
    output_speaker: output_speaker_id
    sample_rate: 48000
```

## Configuration variables

- **output_speaker** (**Required**, [ID](/guides/configuration-types#id)): The {{< docref "/components/speaker/index" "speaker" >}} to output the resampled audio.
- **buffer_duration** (*Optional*, [Time](/guides/configuration-types#time)): The duration of the internal ring buffer. Larger values may reduce stuttering but use more memory. Defaults to `500ms`.
- **bits_per_sample** (*Optional*, positive integer): The audio sample bit depth after resampling. Defaults to the output speaker's bits per sample.
- **sample_rate** (*Optional*, positive integer): Sample rate to convert to. Must be between `8000` and `48000`. Defaults to the output speaker's sample rate.
- **filters** (*Optional*, positive integer): The number of windowed sinc interpolation filters to use. Must be between `2` and `1024`. Defaults to `16`.
- **taps** (*Optional*, positive integer): The number of taps per windowed sinc interpolation filter. Must between `16` and `128` and divisible by 4. Defaults to `16`.
- **task_stack_in_psram** (*Optional*, boolean): Only with `esp-idf`. Run the audio tasks in external memory. Defaults to `false`.
- All other options from [Speaker Component](/components/speaker#config-speaker).

## Improving quality

Resampling is processor intensive and should be avoided as much as possible. The audio quality is effected by the number of filters and the number of taps. Increasing the number of filters will increase the memory load. Increasing the number of taps will increase the CPU load.

## See also

- [ART Audio Resampler (GitHub)](https://github.com/dbry/audio-resampler)
- {{< docref "index/" >}}
