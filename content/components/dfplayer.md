---
description: "Instructions for setting up DF Player Mini component in ESPHome."
title: "DF-Player mini"
params:
  seo:
    description: Instructions for setting up DF Player Mini component in ESPHome.
    image: dfplayer.svg
---

The `dfplayer` ([datasheet](https://wiki.dfrobot.com/DFPlayer_Mini_SKU_DFR0299)), component
allows you to play sound and music stored in an SD card or USB flash drive.

{{< img src="dfplayer-full.jpg" alt="Image" caption="DF-Player mini Module." width="50.0%" class="align-center" >}}

For this component to work you need to have set up a [UART bus](/components/uart) in your configuration.

## Overview

The module can be powered by the 3.3V output of a NodeMCU. For communication you can connect only
the `tx_pin` of the `uart` bus to the module's `RX` but if you need feedback of playback active
you will also need to connect the `rx_pin` to the module's `TX`.
For best quality audio a powered stereo speaker can be connected to the modules `DAC_R`,
`DAC_L` and `GND`, alternatively the module features a built-in 3W audio amplifier, in that case
the pins `SPK_1` and `SPK_2` should be connected to one passive speaker and a 5V 1A power supply
will be required.

```yaml
# Example configuration entry
dfplayer:
```

## Configuration variables

- **uart_id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID of the UART hub.
- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.
- **on_finished_playback** (*Optional*, [Automation](/automations)): An action to be
  performed when playback is finished.

## `dfplayer.is_playing` Condition

This Condition returns true while playback is active.

```yaml
# In some trigger:
on_...:
  if:
    condition:
      dfplayer.is_playing
    then:
      logger.log: 'Playback is active!'
```

## `dfplayer.play_next` Action

Starts playback of next track or skips to the next track.

```yaml
on_...:
  then:
    - dfplayer.play_next:
```

## `dfplayer.play_previous` Action

Plays the previously played track.

```yaml
on_...:
  then:
    - dfplayer.play_previous:
```

## `dfplayer.play` Action

Plays a track.

```yaml
on_...:
  then:
    - dfplayer.play:
        file: 23
        loop: false
    # Shorthand
    - dfplayer.play: 23
```

Configuration options:

- **file** (*Optional*, int, [templatable](/automations/templates)): The global track
  number (from all tracks in the device). If not specified plays the first track.

- **loop** (*Optional*, boolean, [templatable](/automations/templates)): Repeats playing
  the same track. Defaults to `false`.

## `dfplayer.play_mp3` Action

Plays a track inside the folder `mp3`. Files inside the folder must be numbered from 1
to 9999, like `0001.mp3`, `0002.mp3`, ... etc.
The folder name needs to be `mp3`, placed under the SD card root directory, and the
mp3 file name needs to be 4 digits, for example, "0001.mp3", placed under the mp3 folder.
If you want, you can add additional text after the number in the filename, for example,
`0001hello.mp3`, but must always be referenced by number only in yaml.

```bash
/mp3
  /0001hello.mp3
  /0002.mp3
  /0003_thisistheway.mp3
  ..
```

```yaml
on_...:
  then:
    - dfplayer.play_mp3:
        file: 1
    # Shorthand
    - dfplayer.play_mp3: 1
```

Configuration options:

- **file** (**Required**, int, [templatable](/automations/templates)): The file number
  inside the `mp3` folder to play.

## `dfplayer.play_folder` Action

Plays files inside numbered folders, folders must be numbered from 1 and with leading
zeros. Like `01`, `02`, ... etc. Files inside the folders must be numbered with two
leading zeros, like `001.mp3`, `002.mp3`, ... etc.
Folder numbers can range from 1 to 99 and file name from 1 to 255 or folder number
from 1 to 10 and file number from 1 to 1000.

```bash
/01
  /001.mp3
  /002.mp3
  ..
/02
  /001.mp3
  /002.mp3
  /003.mp3
  ..
```

```yaml
on_...:
  then:
    - dfplayer.play_folder:
        folder: 2
        file: 1
```

Configuration options:

- **folder** (**Required**, int, [templatable](/automations/templates)): The folder number.
- **file** (*Optional*, int, [templatable](/automations/templates)): The file number
  inside the folder to play. Optional only if `loop` is not set.

- **loop** (*Optional*, boolean, [templatable](/automations/templates)): Repeats playing
  all files in the folder. Causes `file` to be ignored. Defaults to `false`.

## `dfplayer.set_device` Action

Changes the device in use. Valid values are `TF_CARD` and `USB`.

```yaml
on_...:
  then:
    - dfplayer.set_device: TF_CARD
```

## `dfplayer.set_volume` Action

Changes volume.

```yaml
on_...:
  then:
    - dfplayer.set_volume:
        volume: 20
    # Shorthand
    - dfplayer.set_volume: 20
```

Configuration options:

- **volume** (**Required**, int, [templatable](/automations/templates)): The volume value.
  Valid values goes from `0` to `30`.

## `dfplayer.volume_up` Action

Turn volume up.

```yaml
on_...:
  then:
    - dfplayer.volume_up
```

## `dfplayer.volume_down` Action

Turn volume down.

```yaml
on_...:
  then:
    - dfplayer.volume_down
```

## `dfplayer.set_eq` Action

Changes audio equalization preset.

```yaml
on_...:
  then:
    - dfplayer.set_eq:
        eq_preset: ROCK
    # Shorthand
    - dfplayer.set_eq: ROCK
```

Configuration options:

- **eq_preset** (**Required**): Eq Preset value. Valid values are `NORMAL`, `POP`, `ROCK`, `JAZZ`,
  `CLASSIC` and `BASS`.

## `dfplayer.sleep` Action

Enters sleep mode. Playback is stopped and the action `dfplayer.set_device: TF_CARD` should be
send for playback to be enabled again.

```yaml
on_...:
  then:
    - dfplayer.sleep
```

## `dfplayer.reset` Action

Module reset.

```yaml
on_...:
  then:
    - dfplayer.reset
```

## `dfplayer.start` Action

Starts playing a track or resumes paused playback.

```yaml
on_...:
  then:
    - dfplayer.start
```

## `dfplayer.pause` Action

Pauses playback, playback can be resumed from the same position with `dfplayer.start`.

```yaml
on_...:
  then:
    - dfplayer.pause
```

## `dfplayer.stop` Action

Stops playback.

```yaml
on_...:
  then:
    - dfplayer.stop
```

## `dfplayer.random` Action

Randomly plays all tracks.

```yaml
on_...:
  then:
    - dfplayer.random
```

## All actions

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID of the DFPlayer if you have multiple components.

## Test setup

With the following code you can quickly setup a node and use Home Assistant's service in the developer tools.
E.g. for calling `dfplayer.play_folder` select the service `esphome.test_node_dfplayer_play` and in
service data enter

```json
{ "file": 23 }
```

### Sample code

```yaml
uart:
  tx_pin: GPIOXX
  rx_pin: GPIOXX
  baud_rate: 9600

dfplayer:
  on_finished_playback:
    then:
      logger.log: 'Playback finished event'

api:
  actions:
  - action: dfplayer_next
    then:
      - dfplayer.play_next:
  - action: dfplayer_previous
    then:
      - dfplayer.play_previous:
  - action: dfplayer_play
    variables:
      file: int
    then:
      - dfplayer.play: !lambda 'return file;'
  - action: dfplayer_play_loop
    variables:
      file: int
      loop_: bool
    then:
      - dfplayer.play:
          file: !lambda 'return file;'
          loop: !lambda 'return loop_;'
  - action: dfplayer_play_folder
    variables:
      folder: int
      file: int
    then:
      - dfplayer.play_folder:
          folder: !lambda 'return folder;'
          file: !lambda 'return file;'

  - action: dfplayer_play_loop_folder
    variables:
      folder: int
    then:
      - dfplayer.play_folder:
          folder: !lambda 'return folder;'
          loop: true

  - action: dfplayer_set_device_tf
    then:
      - dfplayer.set_device: TF_CARD

  - action: dfplayer_set_device_usb
    then:
      - dfplayer.set_device: USB

  - action: dfplayer_set_volume
    variables:
      volume: int
    then:
      - dfplayer.set_volume: !lambda 'return volume;'
  - action: dfplayer_set_eq
    variables:
      preset: int
    then:
      - dfplayer.set_eq: !lambda 'return static_cast<dfplayer::EqPreset>(preset);'

  - action: dfplayer_sleep
    then:
      - dfplayer.sleep

  - action: dfplayer_reset
    then:
      - dfplayer.reset

  - action: dfplayer_start
    then:
      - dfplayer.start

  - action: dfplayer_pause
    then:
      - dfplayer.pause

  - action: dfplayer_stop
    then:
      - dfplayer.stop

  - action: dfplayer_random
    then:
      - dfplayer.random

  - action: dfplayer_volume_up
    then:
      - dfplayer.volume_up

  - action: dfplayer_volume_down
    then:
      - dfplayer.volume_down
```

## See Also

- {{< apiref "dfplayer/dfplayer.h" "dfplayer/dfplayer.h" >}}
