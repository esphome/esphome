---
description: "Instructions for setting up touch screen emulation with the sdl display driver."
title: "SDL2 Touch Screen Emulator"
params:
  seo:
    description: Instructions for setting up touch screen emulation with the sdl display driver.
---

{{< anchor "sdl_touchscreen" >}}

The `sdl` touchscreen platform allows emulating a touch screen by using the mouse with the `sdl` display driver.
The `sdl` display component must be configured to use this.

## Base Touchscreen Configuration

```yaml
# Example configuration entry
touchscreen:
  platform: sdl
```

### Configuration variables

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually set the ID of this touchscreen.

- All other options from [Touchscreen](/components/touchscreen#config-touchscreen).

## See Also

- [SDL display](/components/display/sdl#sdl)
- {{< apiref "sdl/sdl_touchscreen.h" "sdl/sdl_touchscreen.h" >}}
