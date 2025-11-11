---
description: "Instructions for setting up quad SPI displays."
title: "Quad SPI Displays"
params:
  seo:
    description: Instructions for setting up quad SPI displays.
    image: t4-s3.jpg
---

{{< anchor "qspi_dbi" >}}

## Models

This display driver supports AMOLED and LCD displays with quad SPI interfaces, using the MIPI DBI interface.

> [!WARNING]
> This component has been made redundant since this class of displays is now supported by the {{< docref "mipi_spi" >}}
> This component will be removed in a future release.

This driver has been tested with the following displays:

- Lilygo T4-S3
- Lilygo T-Display S3 AMOLED
- JC4832W535 board
- JC3636W518 board

## Usage

This component requires an ESP32 and the use of
ESP-IDF. PSRAM is a requirement due to the size of the display buffer. A [quad SPI bus](/components/spi) interface must be configured.

{{< img src="t4-s3.jpg" alt="Image" caption="Lilygo T4-S3" width="75.0%" class="align-center" >}}

{{< img src="t-display-amoled.jpg" alt="Image" caption="Lilygo T-Display S3 AMOLED" width="75.0%" class="align-center" >}}

```yaml
# Example configuration entry

display:
  - platform: qspi_dbi
    model: RM690B0
    data_rate: 80MHz
    dimensions:
      width: 450
      height: 600
      offset_width: 16
    brightness: 255
    cs_pin: GPIOXX
    reset_pin: GPIOXX
    enable_pin: GPIOXX
```

### Configuration variables

- **model** (**Required**): One of

  - `CUSTOM`
  - `RM67162`
  - `RM690B0`
  - `JC4832W535`
  - `JC3636W518`
  - `AXS15231`

- **init_sequence** (*Optional*, A list of byte arrays): Specifies the init sequence for the display. This is required when using the `CUSTOM` model - but may be empty. If specified for other models this data will be sent after the pre-configured sequence.
- **cs_pin** (**Required**, [Pin Schema](/guides/configuration-types#pin-schema)): The chip select pin.
- **reset_pin** (*Optional*, [Pin Schema](/guides/configuration-types#pin-schema)): The RESET pin.
- **enable_pin** (*Optional*, [Pin Schema](/guides/configuration-types#pin-schema)): The display enable pin.
- **brightness** (*Optional*, int): A brightness value in the range 0-255
- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to re-draw the screen. Defaults to `5s`.
- **auto_clear_enabled** (*Optional*, boolean): Whether to automatically clear the display data before each lambda call,
  or to keep the existing display content (must overwrite explicitly, e.g., only on data change). Defaults to `true` if a lambda or pages are configured, false otherwise.

- **pages** (*Optional*, list): Show pages instead of a single lambda. See [Display Pages](/components/display#display-pages).
- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.
- **color_order** (*Optional*): Should be one of `rgb` (default) or `bgr`.
- **dimensions** (**Required**): Dimensions of the screen, specified either as *width* **x** *height* (e.g `320x240`  ) or with separate config keys.
  - **height** (**Required**, int): Specifies height of display in pixels.
  - **width** (**Required**, int): Specifies width of display.
  - **offset_width** (*Optional*, int): Specify an offset for the x-direction of the display, typically used when a display is smaller than the maximum supported by the driver chip. Default is 0
  - **offset_height** (*Optional*, int): Specify an offset for the y-direction of the display. Default is 0.

- **rotation** (*Optional*): Rotate the display presentation in software. Choose one of `0°`, `90°`, `180°`, or `270°`.
- **transform** (*Optional*): Transform the display presentation using hardware. All defaults are `false`. This option cannot be used with `rotation`.

  - **swap_xy** (*Optional*, boolean): If true, exchange the x and y axes. Not available for some chips
  - **mirror_x** (*Optional*, boolean): If true, mirror the x axis.
  - **mirror_y** (*Optional*, boolean): If true, mirror the y axis.
- **data_rate** (*Optional*, int): Set the data rate of the SPI interface to the display. One of `80MHz`, `40MHz`, `20MHz`, `10MHz` (default), `5MHz`, `2MHz` or `1MHz`.
- **spi_mode** (*Optional*): Set the mode for the SPI interface to the display. Default is `MODE0`.
- **invert_colors** (*Optional*, boolean): With this boolean option you can invert the display colors.
- **draw_from_origin** (*Optional*, boolean): When set, all partial display updates will start at the origin (0,0). Defaults to false.
- **draw_rounding** (*Optional*, int): Caters for display chips that require partial drawing to be aligned to certain boundaries. Default is 2, must be a power of 2.
- **lambda** (*Optional*, [lambda](/automations/templates#config-lambda)): The lambda to use for rendering the content on the display.
  See [Display Rendering Engine](/components/display#display-engine) for more information.

## Example configurations

### Lilygo T4-S3

```yaml
spi:
  id: quad_spi
  type: quad
  clk_pin: 15
  data_pins: [14, 10, 16, 12]

i2c:
  sda: 6
  scl: 7

touchscreen:
  - platform: cst226
    id: my_touchscreen
    interrupt_pin: 8
    reset_pin: 17

display:
  - platform: qspi_dbi
    model: RM690B0
    data_rate: 80MHz
    spi_mode: mode0
    dimensions:
      width: 450
      height: 600
      offset_width: 16
    color_order: rgb
    invert_colors: false
    brightness: 255
    cs_pin: 11
    reset_pin: 13
    enable_pin: 9

psram:
  mode: octal
  speed: 80MHz
```

### Lilygo T-Display S3 AMOLED

```yaml
spi:
  id: quad_spi
  type: quad
  clk_pin: 47
  data_pins:
    - 18
    - 7
    - 48
    - 5

i2c:
  sda: 3
  scl: 2

touchscreen:
  - platform: cst816
    id: my_touchscreen
    interrupt_pin:
      number: 21

display:
  - platform: qspi_dbi
    model: RM67162
    id: main_lcd
    dimensions:
      height: 240
      width: 536
    transform:
      mirror_x: true
      swap_xy: true
    color_order: rgb
    brightness: 255
    cs_pin: 6
    reset_pin: 17
    enable_pin: 38
```

### JC4832W535 3.5" LCD Board

This rotates the display into landscape mode using software rotation.

```yaml
psram:
  mode: octal
  speed: 80MHz

spi:
  id: display_qspi
  type: quad
  clk_pin: 47
  data_pins: [21,48,40,39]

power_supply:
  id: backlight_id
  pin: 1
  enable_on_boot: true

display:
  - platform: qspi_dbi
    model: JC4832W535
    data_rate: 40MHz
    rotation: 270
    dimensions:
      height: 480
      width: 320
    cs_pin:
      number: 45
      ignore_strapping_warning: true
    show_test_card: true

i2c:
  sda: 4
  scl: 8

touchscreen:
  platform: axs15231
  transform:
    swap_xy: true
    mirror_y: true
```

## See Also

- {{< docref "index/" >}}
- {{< apiref "qspi_dbi/qspi_dbi.h" "qspi_dbi/qspi_dbi.h" >}}
