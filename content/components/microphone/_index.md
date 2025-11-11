---
description: "Instructions for setting up microphones in ESPHome."
title: "Microphone Components"
params:
  seo:
    description: Instructions for setting up microphones in ESPHome.
    image: folder-open.svg
---

The `microphone` domain contains common functionality shared across the
microphone platforms.

{{< anchor "config-microphone" >}}

## Base Microphone Configuration

Configuration variables:

- **on_data** (*Optional*, [Automation](/automations)): An automation to
  perform when new data is received.

{{< anchor "config-microphone-source" >}}

## Microphone Source Configuration

A microphone source configuration is used by components to ensure that it receives audio in the required format.

Configuration variables:

- **microphone** (**Required**, [ID](/guides/configuration-types#id)): The {{< docref "/components/microphone/index" "microphone" >}} to use for input.
- **bits_per_sample** (*Optional*, int): The bits per sample to use as input to the component.
  May be restricted by the component to a specific value.

- **channels** (*Optional*, list): A list of 0-indexed channel numbers enabling them to use as
  input to the component. The total amount may be restricted by the component. Defaults to 0,
  the first channel read by the microphone.

- **gain_factor** (*Optional*, int): The gain factor to apply to audio read from the microphone. Ranges from 1 to 64.
  Defaults to 1, no gain.

{{< anchor "microphone-actions" >}}

## Microphone Actions

All `microphone` actions can be used without specifying an `id` if you have only one `microphone` in
your configuration YAML.

Configuration variables:

**id** (*Optional*, [ID](/guides/configuration-types#id)): The microphone to control. Defaults to the only one in YAML.

{{< anchor "microphone-capture" >}}

### `microphone.capture` Action

This action will start capturing audio data from the microphone. The data will be passed to any components listening
and will be available in the `on_data` trigger.

{{< anchor "microphone-stop_capture" >}}

### `microphone.stop_capture` Action

This action will stop capturing audio data from the microphone.

{{< anchor "microphone-mute" >}}

### `microphone.mute` Action

This action will apply a software mute to the audio data from the microphone before passing it to any listening components.

{{< anchor "microphone-unmute" >}}

### `microphone.unmute` Action

This action will disable applying a software mute initiated with `microphone.mute`.

{{< anchor "microphone-triggers" >}}

## Microphone Triggers

{{< anchor "microphone-on_data" >}}

### `microphone.on_data` Trigger

This trigger will fire when new data is received from the microphone.
The data is available as a `std::vector<uint8_t>` in the variable `x`.
This data is the raw microphone audio and includes all the read bits per sample and channels.

```yaml
microphone:
  - platform: ...
    on_data:
      - logger.log:
          format: "Received %d bytes"
          args: ['x.size()']
```

Configuration variables:

- **id** (*Optional*, [ID](/guides/configuration-types#id)): The microphone to check. Defaults to the only one in YAML.

{{< anchor "microphone-conditions" >}}

## Microphone Conditions

All `microphone` conditions can be used without specifying an `id` if you have only one `microphone` in
your configuration YAML.

Configuration variables:

**id** (*Optional*, [ID](/guides/configuration-types#id)): The microphone to check. Defaults to the only one in YAML.

{{< anchor "microphone-is_capturing" >}}

### `microphone.is_capturing` Condition

This condition will check if the microphone is currently capturing audio data.

{{< anchor "microphone-is_muted" >}}

### `microphone.is_muted` Condition

This condition will check if the microphone is currently apply a software mute.

## Platforms

## See Also
