---
description: "Instructions for setting up ePaper SPI displays in ESPHome."
title: "ePaper SPI Display"
params:
  seo:
    description: Instructions for setting up ePaper SPI displays with improved architecture in ESPHome.
    image: epaper.svg
---

The `epaper_spi` display platform provides a new ePaper display component architecture
with improved state management and non-blocking operation. This component implements a
queue-based state machine that eliminates blocking waits for the busy pin and provides
better integration with ESPHome's async architecture.

The communication method uses [SPI](/components/spi), so you need to have an `spi:` section in your
configuration.

The driver supports a number of displays and there are also specific configurations for ESP32 boards with integrated displays.
For those boards the predefined configuration will set the correct pins and dimensions for the display.

```yaml
display:
  - platform: epaper_spi
    model: Seeed-reTerminal-E1002
    lambda: |-
      it.filled_circle(it.get_width() / 2, it.get_height() / 2, 50, Color::BLACK);
```

## Supported display controllers

These are the supported controller chips. Using just the chip name as the model will require full configuration with
pins and dimensions specified.

| Chip name              | Manufacturer | Product Description                                                                                                          |
|------------------------|--------------|------------------------------------------------------------------------------------------------------------------------------|
| Spectra-E6             | Eink         | <https://www.eink.com/brand/detail/Spectra6>                                                                                 |
| SSD1677                | Solomon      | <https://www.solomon-systech.com/product/ssd1677/>                                                                           |

## Supported integrated display boards

These models correspond to displays integrated with a microcontroller, and have a full configuration predefined, so at
a minimum only the model name need be configured. Other options can be overridden in the configuration if needed.

| Model name             | Manufacturer | Product Description                                                                                                          |
| ---------------------- |--------------| ---------------------------------------------------------------------------------------------------------------------------- |
| Seeed-reTerminal-E1002 | Seeed Studio | <https://www.seeedstudio.com/reTerminal-E1002-p-6533.html>                                                                   |
| Seeed-ee04-mono-4.26   | Seeed Studio | Seeed EE04 board with Waveshare 4.26" mono epaper. <https://www.seeedstudio.com/XIAO-ePaper-Display-Board-EE04-p-6560.html>  |

## Configuration variables

When using a model defining an integrated display board most of the configuration such as the pins and dimensions will be set by default,
but can be overridden if needed.

- **model** (**Required**): The model of the ePaper display. See the table above for options (case is not significant).
- **cs_pin** (**Required**, [Pin Schema](/guides/configuration-types#pin-schema)): The CS pin. Predefined for integrated boards.
- **dc_pin** (**Required**, [Pin Schema](/guides/configuration-types#pin-schema)): The DC pin. Predefined for integrated boards.
- **busy_pin** (*Optional*, [Pin Schema](/guides/configuration-types#pin-schema)): The BUSY pin, if used.
- **reset_pin** (*Optional*, [Pin Schema](/guides/configuration-types#pin-schema)): The RESET pin, if used.
  Make sure you pull this pin high (by connecting it to 3.3V with a resistor) if not connected to a GPIO pin.
- **dimensions** (**Required**, dict): Dimensions of the screen, specified either as *width* **x** *height* (e.g `320x240`  )
  or with separate config keys. For integrated boards with full pre-defined configuration this is optional and will be preset by
  the model selected. The dimensions are specified in pixels, and the width and height must be greater than 0.

  - **height** (**Required**, int): Specifies height of display.
  - **width** (**Required**, int): Specifies width of display.

- **rotation** (*Optional*, int): Set the rotation of the display. Everything you draw in `lambda:` will be rotated
  by this option. One of `0°` (default), `90°`, `180°`, `270°`.
- **transform** (*Optional*, dict): If `rotation` is not sufficient, use this to transform the display. Options are:
  - **mirror_x** (**Required**, boolean): If true, mirror the x axis.
  - **mirror_y** (**Required**, boolean): If true, mirror the y axis.

- **reset_duration** (*Optional*, [Time](/guides/configuration-types#time)): Duration for the display reset operation. Defaults to `200ms`.
- **lambda** (*Optional*, [lambda](/automations/templates#config-lambda)): The lambda to use for rendering the content on the display.
  See [Display Rendering Engine](/components/display#display-engine) for more information.
- **pages** (*Optional*, list): Show pages instead of a single lambda. See [Display Pages](/components/display#display-pages).
- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to re-draw the screen. Defaults to `60s`,
  use `never` to only manually update the screen via `component.update`.
- **full_update_every** (*Optional*, int): On screens that support partial updates, this sets the number of updates
  before a full update is forced. Defaults to `1` which will make every update a full update.
- **spi_id** (*Optional*, [ID](/guides/configuration-types#id)): Required to specify the ID of the [SPI Component](/components/spi) if your
  configuration defines multiple SPI buses. If only a single SPI bus is configured, this is optional.
- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.

### Full configuration example

```yaml
display:
  - platform: epaper_spi
    model: SSD1677
    full_update_every: 10
    update_interval: 5s
    dimensions:
      width: 800
      height: 480
    transform:
      mirror_x: true
      mirror_y: false
    rotation: 90 # Rotate to portrait
    cs_pin: GPIOXX
    dc_pin: GPIOXX
    reset_pin: GPIOXX
    busy_pin: { number: GPIOXX, inverted: False, mode: { input: True, pulldown: True } }
```

## See Also

- {{< apiref "epaper_spi/epaper_spi.h" "epaper_spi/epaper_spi.h" >}}
- [ESPHome Display Rendering Engine](/components/display#display-engine)
- {{< docref "components/lvgl" >}}
