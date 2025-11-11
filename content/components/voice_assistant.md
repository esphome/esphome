---
description: "Instructions for setting up a Voice Assistant in ESPHome."
title: "Voice Assistant"
params:
  seo:
    description: Instructions for setting up a Voice Assistant in ESPHome.
    image: voice-assistant.svg
---

ESPHome devices with a microphone are able to stream the audio to Home Assistant and be processed there by
[assist](https://www.home-assistant.io/voice_control/).

> [!NOTE]
> Voice Assistant requires Home Assistant 2023.5 or later.

> [!WARNING]
> Audio and voice components consume a significant amount of resources (RAM, CPU) on the device.
>
> **Crashes are likely to occur** if you include too many additional components in your device's
> configuration. In particular, Bluetooth/BLE components are known to cause issues when used in
> combination with Voice Assistant and/or other audio components.
>
> If you experience crashes, see the {{< docref "/guides/troubleshooting" >}} guide for how to get a backtrace.

```yaml
voice_assistant:
  microphone: mic_id
```

## Configuration variables

- **microphone** (**Required**, [Microphone Source Configuration](/components/microphone#config-microphone-source)): The
  {{< docref "/components/microphone/index" "microphone" >}} settings to use for input.

- **micro_wake_word** (*Optional*, [ID](/guides/configuration-types#id)): The {{< docref "/components/micro_wake_word" "micro_wake_word" >}}
  component used for wake word detection. Configuring this allows Home Assistant to change which wake word model is enabled.

- **speaker** (*Optional*, [ID](/guides/configuration-types#id)): The {{< docref "/components/speaker/index" "speaker" >}} to use to output
  the response. Cannot be used with `media_player` below.

- **media_player** (*Optional*, [ID](/guides/configuration-types#id)): The {{< docref "/components/media_player/index" "media_player" >}}
  to use to output the response. Cannot be used with `speaker` above.

- **use_wake_word** (*Optional*, boolean): Enable wake word on the assist pipeline. Defaults to `false`.
- **conversation_timeout** (*Optional*, [Time](/guides/configuration-types#time)): How long to wait before resetting the `conversation_id`
  sent to the voice assist pipeline, which contains the context of the current assist pipeline. Defaults to `300s`.

- **on_intent_start** (*Optional*, [Automation](/automations)): An automation to perform when intent processing starts.
- **on_intent_progress** (*Optional*, [Automation](/automations)): An automation to perform when intent progress happens.
  The variable `x` is a non-empty string containing the streaming TTS response URL only if it is sent to the media player.

- **on_intent_end** (*Optional*, [Automation](/automations)): An automation to perform when intent processing ends.
- **on_listening** (*Optional*, [Automation](/automations)): An automation to
  perform when the voice assistant microphone starts listening.

- **on_start** (*Optional*, [Automation](/automations)): An automation to
  perform when the assist pipeline is started.

- **on_wake_word_detected** (*Optional*, [Automation](/automations)): An automation
  to perform when the assist pipeline has detected a wake word.

- **on_end** (*Optional*, [Automation](/automations)): An automation to perform
  when the voice assistant is finished all tasks.

- **on_stt_end** (*Optional*, [Automation](/automations)): An automation to perform
  when the voice assistant has finished speech-to-text. The resulting text is
  available to automations as the variable `x`.

- **on_stt_vad_start** (*Optional*, [Automation](/automations)): An automation to perform when voice activity
  detection starts speech-to-text processing.

- **on_stt_vad_end** (*Optional*, [Automation](/automations)): An automation to perform when voice activity
  detection ends speech-to-text processing.

- **on_tts_start** (*Optional*, [Automation](/automations)): An automation to perform
  when the voice assistant has started text-to-speech. The text to be spoken is
  available to automations as the variable `x`.

- **on_tts_end** (*Optional*, [Automation](/automations)): An automation to perform
  when the voice assistant has finished text-to-speech. A URL containing the audio response
  is available to automations as the variable `x`.

- **on_tts_stream_start** (*Optional*, [Automation](/automations)): An automation to perform when audio stream
  (voice response) playback starts. Requires `speaker` to be configured.

- **on_tts_stream_end** (*Optional*, [Automation](/automations)): An automation to perform when audio stream
  (voice response) playback ends. Requires `speaker` to be configured.

- **on_idle** (*Optional*, [Automation](/automations)): An automation to perform
  when the voice assistant is idle (no other actions/states are in progress).

- **on_error** (*Optional*, [Automation](/automations)): An automation to perform
  when the voice assistant has encountered an error. The error code and message are available to
  automations as the variables `code` and `message`.

- **on_client_connected** (*Optional*, [Automation](/automations)): An automation to perform
  when Home Assistant has connected and is waiting for Voice Assistant commands.

- **on_client_disconnected** (*Optional*, [Automation](/automations)): An automation to perform
  when Home Assistant disconnects from the Voice Assistant.

- **noise_suppression_level** (*Optional*, integer): The noise suppression level to apply to the assist pipeline.
  Between 0 and 4 inclusive. Defaults to 0 (disabled).

- **auto_gain** (*Optional*, dBFS): Auto gain level to apply to the assist pipeline.
  Between 0dBFS and 31dBFS inclusive. Defaults to 0 (disabled).

- **volume_multiplier** (*Optional*, float): Volume multiplier to apply to the assist pipeline.
  Must be larger than 0. Defaults to 1 (disabled).

- **on_timer_started** (*Optional*, [Automation](/automations)): An automation to perform when a voice assistant
  timer has started. The timer is available as `timer` of type
  {{< apistruct "voice_assistant::Timer" "voice_assistant::Timer" >}}.

- **on_timer_finished** (*Optional*, [Automation](/automations)): An automation to perform when a voice assistant
  timer has finished. The timer is available as `timer` of type
  {{< apistruct "voice_assistant::Timer" "voice_assistant::Timer" >}}.

- **on_timer_cancelled** (*Optional*, [Automation](/automations)): An automation to perform when a voice assistant
  timer has been cancelled. The timer is available as `timer` of type
  {{< apistruct "voice_assistant::Timer" "voice_assistant::Timer" >}}.

- **on_timer_updated** (*Optional*, [Automation](/automations)): An automation to perform when a voice assistant
  timer has been updated (paused/resumed/duration changed). The timer is available as `timer` of type
  {{< apistruct "voice_assistant::Timer" "voice_assistant::Timer" >}}.

- **on_timer_tick** (*Optional*, [Automation](/automations)): An automation to perform when the voice assistant timers
  tick is triggered.
  This is called every **1 second** while there are timers on this device.
  The timers are available as `timers` which is a `std::vector` (array) of type
  {{< apistruct "voice_assistant::Timer" "voice_assistant::Timer" >}}.

{{< anchor "voice_assistant-actions" >}}

## Actions

The following actions are available for use in automations:

### `voice_assistant.start` Action

Listens for one voice command then stops.

#### Configuration variables

- **silence_detection** (*Optional*, boolean): Enable silence detection. Defaults to `true`.
- **wake_word** (*Optional*, string): The wake word that was used to trigger the voice assistant
  when using on-device wake word such as {{< docref "/components/micro_wake_word" >}}.

Call `voice_assistant.stop` to signal the end of the voice command if `silence_detection` is set to `false`.

### `voice_assistant.start_continuous` Action

Start listening for voice commands. This will start listening again after
the response audio has finished playing. Some errors will stop the cycle.
Call `voice_assistant.stop` to stop the cycle.

### `voice_assistant.stop` Action

Stop listening for voice commands.

## Conditions

The following conditions are available for use in automations:

### `voice_assistant.is_running` Condition

Returns true if the voice assistant is currently running.

### `voice_assistant.connected` Condition

Returns true if the voice assistant is currently connected to Home Assistant.

## Wake word detection

See our [example YAML files on GitHub](https://github.com/esphome/wake-word-voice-assistants/blob/main/m5stack-atom-echo/m5stack-atom-echo.yaml)
for continuous wake word detection.

## Push to Talk

Here is an example offering Push to Talk with a {{< docref "/components/binary_sensor" >}}.

```yaml
voice_assistant:
  microphone:
    microphone: ...
    channels: 0
    gain_factor: 4
  speaker: ...

binary_sensor:
  - platform: gpio
    pin: ...
    on_press:
      - voice_assistant.start:
          silence_detection: false
    on_release:
      - voice_assistant.stop:
```

## Click to Converse

```yaml
voice_assistant:
  microphone:
    microphone: ...
    channels: 0
    gain_factor: 4
  speaker: ...

binary_sensor:
  - platform: gpio
    pin: ...
    on_click:
      - if:
          condition: voice_assistant.is_running
          then:
            - voice_assistant.stop:
          else:
            - voice_assistant.start_continuous:
```

## See Also

- {{< docref "microphone/" >}}
- {{< apiref "voice_assistant/voice_assistant.h" "voice_assistant/voice_assistant.h" >}}
