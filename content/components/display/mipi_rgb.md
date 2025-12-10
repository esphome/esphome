---
description: "Instructions for setting up 16 bit \"RGB\" parallel displays"
title: "MIPI RGB Display Driver"
params:
  seo:
    description: Instructions for setting up 16 bit "RGB" parallel displays
    image: indicator.jpg
---

## Types of Display

This display driver supports displays with 16 bit parallel interfaces, often referred to as "RGB".
Two classes of display fall under this category, the first are those that only have the RGB interface and require
no special configuration of the driver chip. The second are those that have both the RGB interface and an SPI interface
which is used to configure the driver chip.

## Supported boards and driver chips

The driver supports a number of display driver chips, and can be configured for custom displays. As well as support for
driver chips, there are also specific configurations for several ESP32 boards with integrated displays. For those boards
the predefined configuration will set the correct pins and dimensions for the display.

For custom displays, the driver can be configured with the correct pins and dimensions, and the driver chip can be
specified, or a custom init sequence can be provided.

### Driver chips

| Driver Chip | Typical Dimensions |
| ----------- | ------------------ |
| ST7701S     | 480x480            |
| RPI         | varies             |
| CUSTOM      | varies             |

The `RPI` driver chip represents displays without an SPI interface, so no init sequence is required. The `CUSTOM`
model has no predefined config options so requires a full yaml configuration.

### Supported integrated display boards

These boards have completely pre-filled configurations for the display driver, so the only required configuration
option is `model`.

| Board                        | Driver Chip | Manufacturer | Product link                                                     |
|------------------------------|-------------| ------------ |------------------------------------------------------------------|
| GUITION-4848S040             | ST7701s     | Guition      | <https://devices.esphome.io/devices/Guition-ESP32-S3-4848S040>   |
| T-PANEL-S3                   | ST7701s     | Lilygo       | <https://lilygo.cc/products/t-panel-s3>                          |
| T-RGB-2.1                    | ST7701s     | Lilygo       | <https://lilygo.cc/products/t-rgb>                               |
| T-RGB-2.8                    | ST7701s     | Lilygo       | <https://lilygo.cc/products/t-rgb>                               |
| SEEED-INDICATOR-D1           | ST7701s     | Seeed Studio | <https://www.seeedstudio.com/SenseCAP-Indicator-D1L-p-5646.html> |
| ESP32-S3-TOUCH-LCD-4.3       | RPI         | Waveshare    | <https://www.waveshare.com/esp32-s3-touch-lcd-4.3.htm>           |
| ESP32-S3-TOUCH-LCD-7-800X480 | RPI         | Waveshare    | <https://www.waveshare.com/esp32-s3-touch-lcd-7.htm>             |
| WAVESHARE-3.16-320X820       | ST7701s     | Waveshare    | <https://www.waveshare.com/esp32-s3-lcd-3.16.htm>                |
| WAVESHARE-4-480X480          | RPI         | Waveshare    | <https://www.waveshare.com/esp32-s3-touch-lcd-4.htm>             |
| WAVESHARE-5-1024X600         | RPI         | Waveshare    | <https://www.waveshare.com/esp32-s3-touch-lcd-5.htm>             |

## Usage

This component requires an ESP32 (usually an ESP32-S3 because of the number of GPIO pins required) and the use of
ESP-IDF. PSRAM is a requirement due to the size of the display buffer.

{{< img src="indicator.jpg" alt="Image" caption="Sensecap Indicator display" width="75.0%" class="align-center" >}}

```yaml
# Example minimal configuration entry
display:
  - platform: mipi_rgb
    model: WAVESHARE-4-480x480
    id: my_display
```

## Configuration options

- **rotation** (*Optional*): Rotate the display presentation in software. Choose one of `0°`, `90°`, `180°`, or `270°`.
  This option cannot be used with `transform`.

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to re-draw the screen. Defaults to `5s`.
- **auto_clear_enabled** (*Optional*, boolean): If the display should be cleared before each update. Defaults to `true`
  if a lambda or pages are configured, false otherwise.
- **lambda** (*Optional*, [lambda](/automations/templates#config-lambda)): The lambda to use for rendering the content on the display.
  See [Display Rendering Engine](/components/display#display-engine) for more information.
- **pages** (*Optional*, list): Show pages instead of a single lambda. See [Display Pages](/components/display#display-pages).
- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.
- **dimensions** (**Required**): Dimensions of the screen, specified either as *width* **x** *height* (e.g `320x240`)
  or with separate config keys.

  - **height** (**Required**, int): Specifies height of display in pixels.
  - **width** (**Required**, int): Specifies width of display.
  - **offset_width** (*Optional*, int): Specify an offset for the x-direction of the display, typically used when an
    LCD is smaller than the maximum supported by the driver chip. Default is 0
  - **offset_height** (*Optional*, int): Specify an offset for the y-direction of the display. Default is 0.

- **data_pins** (**Required**): A list of pins used for the databus. Specified in 3 groups.

  - **red** (**Required**, [Pin Schema](/guides/configuration-types#pin-schema)): Exactly 5 pins for the red databits, listed from least
    to most significant bit.
  - **green** (**Required**, [Pin Schema](/guides/configuration-types#pin-schema)): Exactly 6 pins for the green databits, listed from
    least to most significant bit.
  - **blue** (**Required**, [Pin Schema](/guides/configuration-types#pin-schema)): Exactly 5 pins for the blue databits, listed from
    least to most significant bit.

- **de_pin** (**Required**, [Pin Schema](/guides/configuration-types#pin-schema)): The DE pin.
- **pclk_pin** (**Required**, [Pin Schema](/guides/configuration-types#pin-schema)): The PCLK pin.
- **hsync_pin** (**Required**, [Pin Schema](/guides/configuration-types#pin-schema)): The Horizontal sync pin.
- **vsync_pin** (**Required**, [Pin Schema](/guides/configuration-types#pin-schema)): The Vertical sync pin.
- **reset_pin** (*Optional*, [Pin Schema](/guides/configuration-types#pin-schema)): The RESET pin.
- **hsync_pulse_width** (*Optional*, int): The horizontal sync pulse width.
- **hsync_front_porch** (*Optional*, int): The horizontal front porch length.
- **hsync_back_porch** (*Optional*, int): The horizontal back porch length.
- **vsync_pulse_width** (*Optional*, int): The vertical sync pulse width.
- **vsync_front_porch** (*Optional*, int): The vertical front porch length.
- **vsync_back_porch** (*Optional*, int): The vertical back porch length.
- **pclk_frequency** (*Optional*): Set the pixel clock speed. Default is 8MHz.
- **pclk_inverted** (*Optional*, bool): If the pclk is active negative (default is True)

The horizontal and vertical `pulse_width` , `front_porch` and `back_porch` values are optional, but will
likely require changing from the default values for a specific display. Refer to the manufacturer's sample code
for suitable values. These specify timing requirements for the display.

## Additional Configuration for non-RPI displays

Displays needing a custom init sequence require an SPI bus to be configured, plus these options:

- **dc_pin** (*Optional*, [Pin Schema](/guides/configuration-types#pin-schema)): The DC pin.
- **data_rate** (*Optional*): Set the data rate of the SPI interface to the display. One of `80MHz` , `40MHz` ,
    `20MHz` , `10MHz` , `5MHz` , `2MHz` , `1MHz` (default), `200kHz` , `75kHz` or `1kHz` .
- **spi_mode** (*Optional*): Set the mode for the SPI interface to the display. Default is `MODE0` but some displays
  require `MODE3` .
- **spi_id** (*Optional*, [ID](/guides/configuration-types#id)): The ID of the SPI interface to use - may be omitted if only one SPI bus
  is configured.
- **init_sequence** (*Optional*, A list of byte arrays): Specifies the init sequence for the display.
  Predefined boards
  have a default init sequence, which can be overridden. A custom board can specify the init sequence using this
  variable (RPI displays should provide an empty sequence in which case the SPI bus is not required.)

- **pixel_mode** (*Optional*, string): Set the pixel mode of the RGB bus interface - one of
  `16bit` (default) or `18bit`.
- **invert_colors** (*Optional*): Inverts the display colors, (white becomes black.) Defaults to false.
- **color_order** (*Optional*): Should be one of `bgr` (default) or `rgb`.
- **transform** (*Optional*): Transform the display presentation using hardware.
  This is typically used only to correct for displays that have x or y drivers wired backwards. To rotate the
  display the `rotation` option is preferred - it will automatically use hardware transform if possible.
  The default values for the `mirror_x` and `mirror_y` options are model dependent.
  For the `CUSTOM` model, use of `transform: disabled` will prevent a `rotation` being translated to a hardware transform.

  - **mirror_x** (*Optional*, boolean): If true, mirror the x-axis.
  - **mirror_y** (*Optional*, boolean): If true, mirror the y-axis.
    **Note:** To rotate the display in hardware by 180 degrees set both `mirror_x` and `mirror_y` to `true`.

The `init_sequence` requires a list of elements, which must be byte arrays providing additional
init commands, each consisting of a command byte followed by zero or more data bytes.

A delay may be specified with `delay <N>ms`

These will be collected and sent to the display via SPI during initialisation. The SPI bus need not be implemented
in hardware (i.e. it may use `interface: software`) and it will be released after initialisation, before the RGB
driver is configured. This caters for boards that use the SPI bus pins as RGB pins.

If copying init sequences from other code, note that the array length should not be included as
it will be inferred from the number of bytes provided. The SLPOUT, PIXFMT, INVON, INVOFF and DISPLAY_ON
commands will be automatically appended based on config options and should not be included in
the init sequence in yaml.

## Example configurations

This is an example of a custom configuration.

```yaml
display:
  - platform: mipi_rgb
    model: custom
    id: rpi_disp
    update_interval: never
    auto_clear_enabled: false
    color_order: RGB
    pclk_frequency: 16MHz
    dimensions:
      width: 800
      height: 480
    reset_pin:
      ch422g:
      number: 3
    enable_pin:
      ch422g:
      number: 2
    de_pin:
      number: 5
    hsync_pin:
      number: 46
      ignore_strapping_warning: true
    vsync_pin:
      number: 3
      ignore_strapping_warning: true
    pclk_pin: 7
    pclk_inverted: true
    hsync_back_porch: 30
    hsync_front_porch: 210
    hsync_pulse_width: 30
    vsync_back_porch: 4
    vsync_front_porch: 4
    vsync_pulse_width: 4
    data_pins:
      red:
        - 1         #r3
        - 2         #r4
        - 42        #r5
        - 41        #r6
        - 40        #r7
      blue:
        - 14        #b3
        - 38        #b4
        - 18        #b5
        - 17        #b6
        - 10        #b7
      green:
        - 39        #g2
        - 0         #g3
        - 45        #g4
        - 48        #g5
        - 47        #g6
        - 21        #g7

    init_sequence:
      - [0x01]
      - [0x3A, 0x66]

```

## See Also

- {{< docref "index/" >}}
- {{< apiref "mipi_rgb/mipi_rgb.h" "mipi_rgb/mipi_rgb.h" >}}
