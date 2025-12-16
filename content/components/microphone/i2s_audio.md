---
description: "Instructions for setting up I²S based microphones in ESPHome."
title: "I²S Audio Microphone"
params:
  seo:
    description: Instructions for setting up I²S based microphones in ESPHome.
    image: i2s_audio.svg
---

The `i2s_audio` microphone platform allows you to receive audio via the {{< docref "/components/i2s_audio" >}}.

This platform only works on ESP32 based chips.

> [!WARNING]
> Audio and voice components consume a significant amount of resources (RAM, CPU) on the device.
>
> **Crashes are likely to occur** if you include too many additional components in your device's
> configuration. In particular, Bluetooth/BLE components are known to cause issues when used in
> combination with Voice Assistant and/or other audio components.

```yaml
# Example configuration entry
microphone:
  - platform: i2s_audio
    id: external_mic
    adc_type: external
    i2s_din_pin: GPIOXX

  - platform: i2s_audio
    id: adc_mic
    adc_type: internal
    adc_pin: GPIOXX
```

## Configuration variables

- **adc_type** (**Required**, enum):

  - `external`  : Use an external ADC connected to the I²S bus.
  - `internal`  : Use the internal ADC of the ESP32. Only supported on ESP32, no variant support.

- **channel** (*Optional*, enum): The channel of the microphone. One of `left`, `right`, or `stereo`. If `stereo`, the output data will
  be twice as big, with each right sample followed by a left sample. Defaults to `right`.

- **sample_rate** (*Optional*, positive integer): I2S sample rate. Defaults to `16000`.
- **bits_per_sample** (*Optional*, enum): The bit depth of audio samples representing real data received from the microphone. One of `8bit`, `16bit`, `24bit`, or `32bit`. Defaults to `32bit`.
- **bits_per_channel** (*Optional*, enum): The bit depth of audio samples actually read from the microphone. One of `8bit`, `16bit`, `24bit`, or `32bit`. Defaults to `32bit`. Setting is ignored if the legacy driver is not used.
- **mclk_multiple** (*Optional*, enum): The multiple of the MCLK frequency to the sample rate. Must be divisible by 3 if using 24 bits per sample. One of `128`, `256`, `384`, `512`. Defaults to `256`.
- **use_apll** (*Optional*, boolean): I2S using APLL as main I2S clock, enable it to get accurate clock. Defaults to `false`.
- **i2s_mode** (*Optional*, enum): The I²S mode to use. One of `primary` (clock driven by the host) or `secondary` (clock driven by the attached device). Defaults to `primary`.
- **i2s_audio_id** (*Optional*, [ID](/guides/configuration-types#id)): The ID of the [I²S Audio](/components/i2s_audio#i2s_audio) you wish to use for this microphone.
- **correct_dc_offset** (*Optional*, boolean): Corrects a DC offset for microphones where the audio signal's average amplitude is not 0. Defaults to `false`.
- All other options from [Microphone](/components/microphone#config-microphone)

## External ADC

- **i2s_din_pin** (**Required**, [Pin Schema](/guides/configuration-types#pin-schema)): The GPIO pin to use for the I²S `DIN/SDIN` *(Data In)* signal, also referred to as `SD/SDATA` *(Serial Data)* or `ADCDAT` *(Analog to Digital Converter Data)*.
- **pdm** (*Optional*, boolean): Set this to `true` if your external ADC uses PDM (Pulse Density Modulation) instead of I²S. Defaults to `false`.

> [!NOTE]
> PDM microphones are only supported on ESP32 and ESP32-S3.

## Internal ADC

> [!NOTE]
> Internal ADC microphones are only supported by the legacy I²S driver on a regular ESP32, not the variants.

- **adc_pin** (**Required**, [Pin Schema](/guides/configuration-types#pin-schema)): The GPIO pin to use for the ADC input.

## Known Devices

### M5Stack Atom Echo

```yaml
microphone:
  - platform: i2s_audio
    adc_type: external
    i2s_din_pin: GPIOXX
    pdm: true
```

### RaspiAudio Muse Luxe

```yaml
microphone:
  - platform: i2s_audio
    i2s_din_pin: GPIOXX
    adc_type: external
    pdm: false
```

### ICS-43434

```yaml
microphone:
  - platform: i2s_audio
    i2s_din_pin: GPIOXX
    adc_type: external
    pdm: false
    sample_rate: 48000
    bits_per_sample: 32bit
    channel: left
    use_apll: true
```

## See also

- {{< docref "index/" >}}
