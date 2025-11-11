---
description: "Instructions for setting up SDL2 display on host"
title: "SDL2 Display on host platform"
params:
  seo:
    description: Instructions for setting up SDL2 display on host
    image: sdl.png
---

{{< anchor "sdl" >}}

## Usage

The `sdl` display platform allows you to use create an ESPHome display on a desktop system running Linux or MacOS.
This is particularly useful for designing display layouts, since compiling and running a host binary is much faster
than compiling for and flashing a microcontroller target system.

```yaml
# Example configuration entry
esphome:
  name: sdl

host:

display:
  - platform: sdl
    show_test_card: true
    dimensions:
      width: 450
      height: 600
```

### Configuration variables

- **lambda** (*Optional*, [lambda](/automations/templates#config-lambda)): The lambda to use for rendering the content on the display.
  See [Display Rendering Engine](/components/display#display-engine) for more information.

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to re-draw the screen. Defaults to `1s`.
- **sdl_options** (*Optional*, string): Build arguments if required to specify include or library paths. Should not be required if SDL2 is properly installed.
- **pages** (*Optional*, list): Show pages instead of a single lambda. See [Display Pages](/components/display#display-pages).
- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.
- **window_options** (*Optional*): Options that affect how the display renders on the host system. All default to false, except position, which defaults to SDL's undefined position

  - **position** (*Optional*):
    - **x** (**Required**, int): X position of the display window in pixels
    - **y** (**Required**, int): Y position of the display window in pixels
  - **borderless** (*Optional*, boolean): Whether to draw the display window with or without borders
  - **always_on_top** (*Optional*, boolean): Whether to always draw the display window above other windows or not
  - **fullscreen** (*Optional*, boolean): Whether to draw the display window in fullscreen or not. This may resize the resolution of the host display to match the SDL display dimensions
  - **skip_taskbar** (*Optional*, boolean): Whether to skip adding a taskbar icon for the display window or not
  - **resizable** (*Optional*, boolean): Whether the display window can be manually resized

> [!NOTE]
> To build with this display you must have the
> [SDL2](https://wiki.libsdl.org/SDL2/Installation) package installed. The Sodium encryption library will
> also be required for any API calls. See below for installation hints.

## MacOS SDL2 Installation

The easiest way to install SDL2 on MacOS is using `homebrew`  :

```sh
brew install sdl2 libsodium
```

It may also be necessary to run the command:

```sh
brew link sdl2 libsodium
```

To ensure that the files are symlinked correctly.
You can check installation with the command `sdl2-config --libs --cflags`.

You will need the XCode command-line tools installed to build for the host platform.

## Linux SDL2 Installation

On Debian/Ubuntu derived Linux systems you can install with `apt`  ; also check that you have the necessary build
tools installed, and you must be using a desktop system with a graphic display.

```sh
apt install libsdl2-dev libsodium-dev build-essential git
```

You can check installation with the command `sdl2-config --libs --cflags`.

## Microsoft Windows

Although SDL2 is supported, natively running ESPHome on Windows isn't easy. However the *Windows Subsystem for Linux* (WSL) can be
used to install and use a Linux development environment on Windows, which will enable use of ESPHome and SDL2 as per the
Linux instructions above. See <https://learn.microsoft.com/en-us/windows/wsl/install> for more information on WSL.

## Build and run

The `esphome run yourfile.yaml` command will compile and automatically run the build file on the `host` platform.

## See Also

- [SDL touchscreen](/components/touchscreen/sdl#sdl_touchscreen)
- {{< docref "/components/binary_sensor/sdl" "SDL binary sensor" >}}
- {{< docref "index/" >}}
- {{< apiref "sdl/sdl_esphome.h" "sdl/sdl_esphome.h" >}}
