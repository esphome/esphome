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

```yaml
display:
  - platform: epaper_spi
    cs_pin: GPIOXX
    dc_pin: GPIOXX
    busy_pin: GPIOXX
    reset_pin: GPIOXX
    model: 7.3in-spectra-e6
    lambda: |-
      it.filled_circle(it.get_width() / 2, it.get_height() / 2, 50, Color::BLACK);
```

## Configuration variables

- **cs_pin** (**Required**, [Pin Schema](/guides/configuration-types#pin-schema)): The CS pin.
- **dc_pin** (**Required**, [Pin Schema](/guides/configuration-types#pin-schema)): The DC pin.
- **model** (**Required**): The model of the ePaper display. Currently supported:

  - `7.3in-spectra-e6` - 7.3" Spectra E6 6-color display (800×480 pixels)

- **busy_pin** (*Optional*, [Pin Schema](/guides/configuration-types#pin-schema)): The BUSY pin. Defaults to not connected.
- **reset_pin** (*Optional*, [Pin Schema](/guides/configuration-types#pin-schema)): The RESET pin. Defaults to not connected.
  Make sure you pull this pin high (by connecting it to 3.3V with a resistor) if not connected to a GPIO pin.

- **rotation** (*Optional*): Set the rotation of the display. Everything you draw in `lambda:` will be rotated
  by this option. One of `0°` (default), `90°`, `180°`, `270°`.

- **reset_duration** (*Optional*, [Time](/guides/configuration-types#time)): Duration for the display reset operation. Defaults to `200ms`.

- **lambda** (*Optional*, [lambda](#config-lambda)): The lambda to use for rendering the content on the display.
  See [Display Rendering Engine](#display-engine) for more information.
- **pages** (*Optional*, list): Show pages instead of a single lambda. See [Display Pages](#display-pages).

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to re-draw the screen. Defaults to `60s`,
  use `never` to only manually update the screen via `component.update`.
- **spi_id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID of the [SPI Component](/components/spi) if you want
  to use multiple SPI buses.
- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.

## See Also

- {{< docref "index/" >}}
- {{< apiref "epaper_spi/epaper_spi.h" "epaper_spi/epaper_spi.h" >}}
- [ESPHome Display Rendering Engine](#display-engine)
