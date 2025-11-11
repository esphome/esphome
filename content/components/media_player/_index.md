---
description: "Instructions for setting up generic media players in ESPHome."
title: "Media Player Components"
params:
  seo:
    description: Instructions for setting up generic media players in ESPHome.
    image: folder-open.svg
---

The `media_player` domain includes all platforms that implement media player
functionality.

> [!NOTE]
> ESPHome media players require Home Assistant 2022.6 or newer.

{{< anchor "config-media_player" >}}

## Base Media Player Configuration

```yaml
media_player:
  - platform: ...
    name: "Media Player Name"
```

Configuration variables:

- **id** (*Optional*, string): Manually specify the ID for code generation. At least one of **id** and **name** must be specified.
- **name** (*Optional*, string): The name of the media player. At least one of **id** and **name** must be specified.

> [!NOTE]
> If you have a [friendly_name](/components/esphome#esphome-configuration_variables) set for your device and
> you want the media player to use that name, you can set `name: None`.

- **icon** (*Optional*, icon): Manually set the icon to use for the
  media player in the frontend.

- **internal** (*Optional*, boolean): Mark this component as internal. Internal components will
  not be exposed to the frontend (like Home Assistant). Only specifying an `id` without
  a `name` will implicitly set this to true.

- **disabled_by_default** (*Optional*, boolean): If true, then this entity should not be added to any client's frontend,
  (usually Home Assistant) without the user manually enabling it (via the Home Assistant UI).
  Defaults to `false`.

- **entity_category** (*Optional*, string): The category of the entity.
  See <https://developers.home-assistant.io/docs/core/entity/#generic-properties>
  for a list of available options. Set to `""` to remove the default entity category.

{{< anchor "media_player-actions" >}}

## Media Player Actions

All `media_player` actions can be used without specifying an `id` if you have only one `media_player` in
your configuration YAML.

The actions `turn_off` and `turn_on` are optional and based on the platform implementing the `supports_turn_off_on` trait.

Configuration variables:

**id** (*Optional*, [ID](/guides/configuration-types#id)): The media player to control. Defaults to the only one in YAML.

{{< anchor "media_player-play" >}}

### `media_player.play` Action

This action will resume playing the media player.

{{< anchor "media_player-play_media" >}}

### `media_player.play_media` Action

This action will start playing the specified media.

```yaml
on_...:
  # Simple
  - media_player.play_media: 'http://media-url/media.mp3'

  # Full
  - media_player.play_media:
      id: media_player_id
      media_url: 'http://media-url/media.mp3'

  # Simple with lambda
  - media_player.play_media: !lambda 'return "http://media-url/media.mp3";'
```

Configuration variables:

**media_url** (**Required**, string): The media url to play.

{{< anchor "media_player-pause" >}}

### `media_player.pause` Action

This action pauses the current playback.

{{< anchor "media_player-stop" >}}

### `media_player.stop` Action

This action stops the current playback.

Configuration variables:

**announcement** (*Optional*, boolean): Whether to target announcements or regular media files, if supported by the media player. Defaults to `false`.

{{< anchor "media_player-toggle" >}}

### `media_player.toggle` Action

This action will pause or resume the current playback.

{{< anchor "media_player-turn_off" >}}

### `media_player.turn_off` Action

This action will turn off the media player.

{{< anchor "media_player-turn_on" >}}

### `media_player.turn_on` Action

This action will turn on the media player.

{{< anchor "media_player-volume_up" >}}

### `media_player.volume_up` Action

This action will increase the volume of the media player.

{{< anchor "media_player-volume_down" >}}

### `media_player.volume_down` Action

This action will decrease the volume of the media player.

{{< anchor "media_player-volume_set" >}}

### `media_player.volume_set` Action

This action will set the volume of the media player.

```yaml
on_...:
  # Simple
  - media_player.volume_set: 50%

  # Full
  - media_player.volume_set:
      id: media_player_id
      volume: 50%

  # Simple with lambda
  - media_player.volume_set: !lambda "return 0.5;"
```

Configuration variables:

**volume** (**Required**, percentage): The volume to set the media player to.

{{< anchor "media_player-on_state_trigger" >}}

### `media_player.on_state` Trigger

This trigger is activated each time the state of the media player is updated
(for example, if the player is stop playing audio or received some command).

```yaml
media_player:
  - platform: i2s_audio  # or any other platform
    # ...
    on_state:
      - logger.log: "State updated!"
```

{{< anchor "media_player-on_play_trigger" >}}

### `media_player.on_play` Trigger

This trigger is activated each time then the media player is started playing.

```yaml
media_player:
  - platform: i2s_audio  # or any other platform
    # ...
    on_play:
      - logger.log: "Playback started!"
```

{{< anchor "media_player-on_pause_trigger" >}}

### `media_player.on_pause` Trigger

This trigger is activated every time the media player pauses playback.

```yaml
media_player:
  - platform: i2s_audio  # or any other platform
    # ...
    on_pause:
      - logger.log: "Playback paused!"
```

{{< anchor "media_player-on_idle_trigger" >}}

### `media_player.on_idle` Trigger

This trigger is activated every time the media player finishes playing.

```yaml
media_player:
  - platform: i2s_audio  # or any other platform
    # ...
    on_idle:
      - logger.log: "Playback finished!"
```

{{< anchor "media_player-on_announcement_trigger" >}}

### `media_player.on_announcement` Trigger

This trigger is activated every time the media player plays an announcement.

```yaml
media_player:
  - platform: i2s_audio  # or any other platform
    # ...
    on_announcement:
      - logger.log: "Announcing!"
```

{{< anchor "media_player-on_turn_off_trigger" >}}

### `media_player.on_turn_off` Trigger

This trigger is activated every time the media player is turned off.

```yaml
media_player:
  - platform: ...  # any platform implementing the `supports_turn_off_on` trait
    # ...
    on_turn_off:
      - logger.log: "Media Player is Turned Off"
```

{{< anchor "media_player-on_turn_on_trigger" >}}

### `media_player.on_turn_on` Trigger

This trigger is activated every time the media player is turned on.

```yaml
media_player:
  - platform: ...  # any platform implementing the `supports_turn_off_on` trait
    # ...
    on_turn_on:
      - logger.log: "Media Player is Turned On"
```

{{< anchor "media_player-is_idle_condition" >}}

### `media_player.is_idle` Condition

This condition checks if the media player is idle.

```yaml
# In some trigger:
on_...:
  if:
    condition:
      media_player.is_idle:
```

{{< anchor "media_player-is_playing_condition" >}}

### `media_player.is_playing` Condition

This condition checks if the media player is playing media.

```yaml
# In some trigger:
on_...:
  if:
    condition:
      media_player.is_playing:
```

{{< anchor "media_player-is_paused_condition" >}}

### `media_player.is_paused` Condition

This condition checks if the media player is paused.

```yaml
# In some trigger:
on_...:
  if:
    condition:
      media_player.is_paused:
```

{{< anchor "media_player-is_announcing_condition" >}}

### `media_player.is_announcing` Condition

This condition checks if the media player is playing an announcement.

```yaml
# In some trigger:
on_...:
  if:
    condition:
      media_player.is_announcing:
```

{{< anchor "media_player-is_off_condition" >}}

### `media_player.is_off` Condition

This condition checks if the media player is turned off.

```yaml
# In some trigger:
on_...:
  if:
    condition:
      media_player.is_off:
```

{{< anchor "media_player-is_on_condition" >}}

### `media_player.is_on` Condition

This condition checks if the media player is turned on.

```yaml
# In some trigger:
on_...:
  if:
    condition:
      media_player.is_on:
```

## Play media in order

You can use wait automation to play files one after the other:

```yaml
# In some trigger:
on_...:
  then:
    - media_player.play_media: 'http://media-url/one.mp3'
    - wait_until:
        media_player.is_idle:
    - media_player.play_media: 'http://media-url/two.mp3'
```

## See Also
