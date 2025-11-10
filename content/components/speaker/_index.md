---
description: "Instructions for setting up speakers in ESPHome."
title: "Speaker Components"
params:
  seo:
    description: Instructions for setting up speakers in ESPHome.
    image: speaker.svg
---

The `speaker` domain contains common functionality shared across the
speaker platforms.

{{< anchor "config-speaker" >}}

## Base Speaker Configuration

```yaml
speaker:
  - platform: ...
```

Configuration variables:

- **audio_dac** (*Optional*, [ID](/guides/configuration-types#id)): The {{< docref "/components/audio_dac/index" "audio DAC" >}} to use for volume control.

{{< anchor "speaker-actions" >}}

## Speaker Actions

All `speaker` actions can be used without specifying an `id` if you have only one `speaker` in
your configuration YAML.

{{< anchor "speaker-play" >}}

### `speaker.play` Action

This action will start playing raw audio data from the speaker.

```yaml
on_...:
  # Static raw audio data
  - speaker.play: [...]

  # Templated, return type is std::vector<uint8_t>
  - speaker.play: !lambda return {...};

  # in case you need to specify the speaker id
  - speaker.play:
      id: my_speaker
      data: [...]
```

Configuration variables:

- **id** (*Optional*, [ID](/guides/configuration-types#id)): The speaker to control. Defaults to the only one in YAML.
- **data** (**Required**, list of bytes): The raw audio data to play.

{{< anchor "speaker-stop" >}}

### `speaker.stop` Action

This action will stop playing audio data from the speaker and discard the unplayed data.

Configuration variables:

- **id** (*Optional*, [ID](/guides/configuration-types#id)): The speaker to control. Defaults to the only one in YAML.

{{< anchor "speaker-finish" >}}

### `speaker.finish` Action

This action will stop playing audio data from the speaker after all data **is** played.

Configuration variables:

- **id** (*Optional*, [ID](/guides/configuration-types#id)): The speaker to control. Defaults to the only one in YAML.

{{< anchor "speaker-mute_on" >}}

### `speaker.mute_on` Action

This action will mute the speaker.

Configuration variables:

- **id** (*Optional*, [ID](/guides/configuration-types#id)): The speaker to control. Defaults to the only one in YAML.

{{< anchor "speaker-mute_off" >}}

### `speaker.mute_off` Action

This action will unmute the speaker.

Configuration variables:

- **id** (*Optional*, [ID](/guides/configuration-types#id)): The speaker to control. Defaults to the only one in YAML.

{{< anchor "speaker-volume_set" >}}

### `speaker.volume_set` Action

This action will set the volume of the speaker.

```yaml
on_...:
  # Simple
  - speaker.volume_set: 50%

  # Full
  - speaker.volume_set:
      id: speaker_id
      volume: 50%

  # Simple with lambda
  -  speaker.volume_set: !lambda "return 0.5;"
```

Configuration variables:

**volume** (**Required**, percentage): The volume to set the speaker to.

{{< anchor "speaker-conditions" >}}

## Speaker Conditions

All `speaker` conditions can be used without specifying an `id` if you have only one `speaker` in
your configuration YAML.

{{< anchor "speaker-is_playing" >}}

### `speaker.is_playing` Condition

This condition will check if the speaker is currently playing audio data.

Configuration variables:

- **id** (*Optional*, [ID](/guides/configuration-types#id)): The speaker to check. Defaults to the only one in YAML.

{{< anchor "speaker-is_stopped" >}}

### `speaker.is_stopped` Condition

This condition will check if the speaker is fully stopped audio data and is in idle mode.

.. note:

Between the time `speaker.is_playing` is false and `speaker.is_stopped` is true the 'speaker' component is closing down structures that where used to play the data correctly. *It better to check if the speaker is stopped then that if it plays.*

Configuration variables:

- **id** (*Optional*, [ID](/guides/configuration-types#id)): The speaker to check. Defaults to the only one in YAML.

## Platforms

## See Also

- {{< docref "/guides/audio_clips_for_i2s" >}}
- {{< docref "/components/speaker/i2s_audio" >}}
