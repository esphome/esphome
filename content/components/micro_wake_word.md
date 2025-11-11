---
description: "Instructions for creating a custom wake word using microWakeWord."
title: "Micro Wake Word"
params:
  seo:
    description: Instructions for creating a custom wake word using microWakeWord.
    image: voice-assistant.svg
---

ESPHome implements an on-device wake word detection framework from [microWakeWord](https://github.com/kahrendt/microWakeWord).
This repository/library allows you to create a custom wake word for your ESPHome device.

The training process is described on the [microWakeWord GitHub repository](https://github.com/kahrendt/microWakeWord).

```yaml
# Shorthand name
micro_wake_word:
  models:
    - model: okay_nabu

# Github shorthand URL
micro_wake_word:
  models:
    - model: github://esphome/micro-wake-word-models/models/v2/okay_nabu.json
```

## Configuration variables

- **microphone** (**Required**, [Microphone Source Configuration](/components/microphone#config-microphone-source)): The {{< docref "/components/microphone/index" "microphone" >}} settings to use for audio input.
- **stop_after_detection** (*Optional*, boolean): Whether to stop the component after detecting a wake word. Defaults to `true`.
- **models** (**Required**, list): The models to use. Only the first model is enabled by default on the first boot. Each model's enabled state is then saved/restored to/from the flash.

  - **id** (*Optional*, [ID](/guides/configuration-types#id)): The optional ID used for the model actions below.
  - **model** (**Required**, string): This can be one of:

    - A simple name of a model that exists in the official [ESPHome Models repository](https://github.com/esphome/micro-wake-word-models).
      e.g. `okay_nabu`.

    - A github shorthand URL to a model JSON file.
      e.g. `github://esphome/micro-wake-word-models/models/okay_nabu.json@main`.

    - A full URL to a model JSON file.
      e.g. `https://github.com/esphome/micro-wake-word-models/raw/main/models/okay_nabu.json`.

  - **probability_cutoff** (*Optional*, percentage): The probability cutoff for the wake word detection.
    If the probability of the wake word is below this value, the wake word is not detected.
    A larger value reduces the number of false accepts but increases the number of false rejections.

  - **sliding_window_size** (*Optional*, int): The size of the sliding window average for the wake word detection. A small value lowers latency but may increase the number of false accepts.
  - **internal** (*Optional*, boolean): The wake word model is internal to the device and won't be able to enabled/disabled in Home Assistant.
- **on_wake_word_detected** (*Optional*, Automation): An automation to perform when the wake word is detected.
  The `wake_word` phrase from the model manifest is provided as a `std::string` to any actions in this automation.

- **vad** (*Optional*, model): Enable a Voice Activity Detection model to reduce false accepts from non-speech sounds.

  - **model** (*Optional*, string): This can be one of:

    - A github shorthand URL to a model JSON file.
      e.g. `github://esphome/micro-wake-word-models/models/v2/vad.json@main`.

    - A full URL to a model JSON file.
      e.g. `https://github.com/esphome/micro-wake-word-models/raw/main/models/v2/vad.json`.

  - **probability_cutoff** (*Optional*, percentage): The probability cutoff for voice activity detection.
    If the probability is below this value, then no wake word will be accepted.
    A larger value reduces the number of false accepts but increases the number of false rejections.

  - **sliding_window_size** (*Optional*, int): The size of the sliding window average for voice activity detection. The average probability is compared to `probability_cutoff` to determine if voice activity is detected.

The `probability_cutoff` and `sliding_window_size` are provided by the JSON file but can be overridden in YAML. A default VAD model is provided with the `vad` configuration variables, but a different model can be overridden in YAML.

## Actions

### `micro_wake_word.start` Action

Starts the wake word detection.

### `micro_wake_word.stop` Action

Stops the wake word detection.

### `micro_wake_word.enable_model` Action

```yaml
on_...:
  then:
    - micro_wake_word.enable_model: model_id
```

Enables the specified model so it can be detected when the component is running.

### `micro_wake_word.disable_model` Action

```yaml
on_...:
  then:
    - micro_wake_word.disable_model: model_id
```

Disables the specified model so it won't be detected when the component is running.

## Conditions

### `micro_wake_word.is_running` Condition

Checks if the component is running to detect wake words.

### `micro_wake_word.model_is_enabled` Condition

Checks if the given model is enabled.

## Example usage

```yaml
micro_wake_word:
  microphone:
    microphone: ...
    channels: 0
    gain_factor: 4
  vad:
  models:
    - model: okay_nabu
      id: okay_nabu_model
    - model: hey_mycroft
      id: hey_mycroft_model
wake_word:
  on_wake_word_detected:
    then:
      - voice_assistant.start:
          wake_word: !lambda return wake_word;
```

## Model JSON

```json
{
  "type": "micro",
  "wake_word": "okay nabu",
  "author": "Kevin Ahrendt",
  "website": "https://www.kevinahrendt.com/",
  "model": "stream_state_internal_quant.tflite",
  "version": 2,
  "micro": {
    "probability_cutoff": 0.97,
    "sliding_window_size": 5,
    "feature_step_size": 10,
    "tensor_arena_size": 22860,
    "minimum_esphome_version": "2024.7"
  }
}
```

The model JSON file contains the following fields that are all **required** unless otherwise specified:

- **type** (string): The type of the model. This should always be `micro`.
- **wake_word** (string): The wake word that the model is trained to detect.
- **author** (string): The name of the author that trained the model.
- **website** (*optional* string): The website of the author.
- **model** (string): The relative or absolute path or URL to the TFLite trained model file.
- **trained_languages** (list of strings): A list of the wake word samples' primary languages/pronunciations used when training.
- **version** (int): The version of the JSON schema. The current version is `2`.
- **micro** (object): The microWakeWord specific configuration.

  - **probability_cutoff** (float): The probability cutoff for the wake word detection.
    If the probability of the wake word is below this value, the wake word is not detected.

  - **sliding_window_size** (int): The size of the sliding window for the wake word detection. Wake words average all probabilities in the sliding window and VAD models use the maximum of all probabilities in the sliding window.
  - **feature_step_size** (int): The step size for the spectrogram feature generation in milliseconds.
  - **tensor_arena_size** (int): The minimum size of the tensor arena in bytes.
  - **minimum_esphome_version** (version): The minimum ESPHome version required to use this model.

## See Also

- {{< docref "voice_assistant/" >}}
- {{< apiref "micro_wake_word/micro_wake_word.h" "micro_wake_word/micro_wake_word.h" >}}
