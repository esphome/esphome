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

The communication method uses 4-wire [SPI](/components/spi), so you need to have an `spi:` section in your
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

## Supported displays

| Model name             | Manufacturer | Product Description                                        |
| ---------------------- | ------------ | ---------------------------------------------------------- |
| Spectra-E6             | Eink         | <https://www.eink.com/brand/detail/Spectra6>               |
| 7.3in Spectra-E6       | Eink         | <https://www.eink.com/product/detail/EL073TF1U5>           |
| Seeed-reTerminal-E1002 | Seeed Studio | <https://www.seeedstudio.com/reTerminal-E1002-p-6533.html> |

## Configuration variables

When using a model defining an integrated ESP32 display board most of the configuration such as the pins and dimensions will be set by default,
but can be overridden if needed.

- **model** (**Required**): The model of the ePaper display. See the table above for options.
- **cs_pin** (**Required**, [Pin Schema](/guides/configuration-types#pin-schema)): The CS pin. Predefined for integrated boards.
- **dc_pin** (**Required**, [Pin Schema](/guides/configuration-types#pin-schema)): The DC pin. Predefined for integrated boards.
- **busy_pin** (*Optional*, [Pin Schema](/guides/configuration-types#pin-schema)): The BUSY pin, if used.
- **reset_pin** (*Optional*, [Pin Schema](/guides/configuration-types#pin-schema)): The RESET pin, if used.
  Make sure you pull this pin high (by connecting it to 3.3V with a resistor) if not connected to a GPIO pin.

- **rotation** (*Optional*): Set the rotation of the display. Everything you draw in `lambda:` will be rotated
  by this option. One of `0°` (default), `90°`, `180°`, `270°`.

- **reset_duration** (*Optional*, [Time](/guides/configuration-types#time)): Duration for the display reset operation. Defaults to `200ms`.

- **lambda** (*Optional*, [lambda](/automations/templates#config-lambda)): The lambda to use for rendering the content on the display.
  See [Display Rendering Engine](/components/display#display-engine) for more information.
- **pages** (*Optional*, list): Show pages instead of a single lambda. See [Display Pages](/components/display#display-pages).

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to re-draw the screen. Defaults to `60s`,
  use `never` to only manually update the screen via `component.update`.
- **spi_id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID of the [SPI Component](/components/spi) if you want
  to use multiple SPI buses.
- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.

## See Also

- {{< docref "index/" >}}
- {{< apiref "epaper_spi/epaper_spi.h" "epaper_spi/epaper_spi.h" >}}
- [ESPHome Display Rendering Engine](/components/display#display-engine)
