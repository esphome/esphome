---
description: "Instructions for setting up an SDL keyboard binary sensor."
title: "SDL Binary Sensor"
params:
  seo:
    description: Instructions for setting up an SDL keyboard binary sensor.
---

The `sdl` binary sensor platform creates a binary sensor from keyboard presses on the host platform.
The sensor will be true when the key is pressed.

## Configuration variables

- **key** (**Required**): The ID of an [SDL key](https://www.libsdl.org/release/SDL-1.2.15/docs/html/sdlkey.html).
- All other variables from [Binary Sensor](/components/binary_sensor#config-binary_sensor).

Example:

```yaml
binary_sensor:
  - platform: sdl
    id: key_id
    key: SDLK_a
```

## See Also

- {{< docref "/components/host" "Host Platform" >}}
- [SDL display](/components/display/sdl#sdl)
