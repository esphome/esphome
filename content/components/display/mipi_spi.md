---
description: "Details for the MIPI SPI display driver component in ESPHome"
title: "MIPI SPI Display Driver"
params:
  seo:
    description: Details for the MIPI SPI display driver component in ESPHome
    image: ili9341.jpg
---

{{< anchor "mipi_spi" >}}

## Introduction

This driver is for displays that use the MIPI DBI interface, implemented over SPI. This is a common interface for many
displays, and is used in many ESP32 based display boards. The driver is designed to be flexible and support a wide range
of displays, and can drive displays via SPI, Quad SPI and 8 bit parallel interfaces.

## Background

The MIPI (Mobile Industry Processor Interface) Alliance publishes various hardware and software interface specifications
including the Display Bus Interface (DBI), which defines serial and parallel hardware interfaces, and the Display Command
Set (DCS), which defines a set of commands for controlling displays. These standards are used in many displays intended
for embedded systems, and this driver addresses those display types.

The display panels controlled by the driver may be of various types, including TFT, IPS, AMOLED, and others. Each driver
chip and panel combination requires a specific set of initialisation commands, and standard initialisation sequences are provided for many common
boards and chips, but the driver is also designed to be customisable in YAML for other displays.

## Supported boards and driver chips

The driver supports a number of display driver chips, and can be configured for custom displays. As well as support for
driver chips, there are also specific configurations for several ESP32 boards with integrated displays. For those boards
the predefined configuration will set the correct pins and dimensions for the display.

For custom displays, the driver can be configured with the correct pins and dimensions, and the driver chip can be
specified, or a custom init sequence can be provided. Displays with 8 bit parallel interfaces are supported by
using an octal SPI bus, so references here to parallel and octal SPI are equivalent.

### Driver chips

| Driver Chip | Typical Dimensions |
| ----------- | ------------------ |
| RM690B0     | 320x240            |
| ILI9341     | 320x240            |
| ILI9481     | 320x480            |
| ILI9486     | 320x480            |
| ILI9488     | 320x480            |
| ILI9488_A   | 320x480            |
| ST7796      | 320x480            |
| ST7789V     | 240x320            |
| GC9A01A     | 240x240            |
| GC9D01N     | 240x240            |
| AXS15231    | 320x240            |
| ST7735      | 128x160            |
| CO5300      | 466x466            |
| CUSTOM      | Customisable       |

### Boards with integrated displays

| Model                                | Manufacturer | Product Description                                               |
| ------------------------------------ | ------------ | ----------------------------------------------------------------- |
| ADAFRUIT-S2-TFT-FEATHER              | Adafruit     | <https://www.adafruit.com/product/6312>                           |
| ADAFRUIT-FUNHOUSE                    | Adafruit     | <https://www.adafruit.com/product/4985>                           |
| M5CORE                               | M5Stack      | <https://docs.m5stack.com/en/core/BASIC%20v2.6> |
| M5CORE2                              | M5Stack      | <https://docs.m5stack.com/en/core/core2> |
| S3BOX                                | Espressif | <https://www.espressif.com/en/products/devkits/esp32-s3-box> |
| S3BOXLITE                            | Espressif | <https://www.espressif.com/en/products/devkits/esp32-s3-box-lite> |
| WAVESHARE-4-TFT                      | Waveshare | <https://www.waveshare.com/4inch-tft-touch-shield.htm> |
| PICO-RESTOUCH-LCD-3.5                | Waveshare | <https://www.waveshare.com/pico-restouch-lcd-3.5.htm> |
| WAVESHARE-ESP32-S3-TOUCH-AMOLED-1.75 | Waveshare | <https://www.waveshare.com/esp32-s3-touch-amoled-1.75.htm> |
| WAVESHARE-ESP32-S3-TOUCH-LCD-3.49    | Waveshare | <https://www.waveshare.com/esp32-s3-touch-lcd-3.49.htm> |
| WT32-SC01-PLUS                       | Wireless-Tag | <https://www.wireless-tag.com/portfolio/wt32-sc01-plus/> |
| ESP32-2432S028                       | Sunton | <https://www.espressif.com/en/products/devkits/esp32-2432s028> |
| JC3248W535                           | Guition | <https://www.aliexpress.com/item/1005007566332450.html> |
| JC3636W518                           | Guition | <https://www.aliexpress.com/item/1005007890666293.html> |
| JC3636W518V2                         | Guition | <https://www.aliexpress.com/item/1005007890666293.html> |
| JC4827W543                           | Guition | <https://www.aliexpress.com/item/1005006729377800.html> |
| LANBON-L8                            | Lanbon | <https://www.lanbon.cn/product/lanbon-l8> |
| T4-S3                                | Lilygo | <https://www.lilygo.cc/products/t4-s3> |
| T-EMBED                              | Lilygo | <https://www.lilygo.cc/products/t-embed> |
| T-DISPLAY                            | Lilygo | <https://www.lilygo.cc/products/t-display> |
| T-DISPLAY-S3                         | Lilygo | <https://www.lilygo.cc/products/t-display-s3> |
| T-DISPLAY-S3-PRO                     | Lilygo | <https://www.lilygo.cc/products/t-display-s3-pro> |
| T-DISPLAY-S3-AMOLED                  | Lilygo | <https://www.lilygo.cc/products/t-display-s3-amoled> |
| T-DISPLAY-S3-AMOLED-PLUS             | Lilygo | <https://www.lilygo.cc/products/t-display-s3-amoled-plus> |

## SPI Bus

An {{< docref "/components/spi" >}} is used to communicate with the display driver chip. The driver supports single bit SPI, quad SPI and octal SPI. The SPI
bus must be configured separately, and must be of the correct type for the display driver chip.

## Configuration

```yaml
# Example minimal configuration entry
display:
  - platform: mipi_spi
    model: T_EMBED
```

### Configuration options

All [graphical display configuration](/components/display#display-configuration) options are available, plus the following. For integrated display boards
most of the configuration will be set by default, but can be overridden if needed.

- **model** (**Required**): Chosen from the lists of supported chips and models above, or `CUSTOM` for custom displays.
- **bus_mode** (*Optional*): Select the SPI bus mode for the display driver. Options are `single` (default), `quad` and `octal`.
- **dc_pin** (**Required**, [Pin Schema](/guides/configuration-types#pin-schema)): The DC pin. Not required or permitted for quad SPI.
- **reset_pin** (*Optional*, [Pin Schema](/guides/configuration-types#pin-schema)): The RESET pin.
- **cs_pin** (*Optional*, [Pin Schema](/guides/configuration-types#pin-schema)): The CS pin.

> [!NOTE]
> A DC pin is required for single SPI and 8 bit parallel, the CS pin and RESET pin will only be needed if the specific board has those
> pins wired to GPIOs. When using a board with integrated display, the pins will be set to the correct values by
> default, but can be overridden in the config if needed.

- **enable_pin** (*Optional*, [Pin Schema](/guides/configuration-types#pin-schema)): An optional pin to enable the display, if required. A list of pins can be provided for displays that require multiple enable pins. A full pin configuration may be provided
  to set the pin mode and inverted property. By default the pin will be driven high to enable the display.

- **brightness** (*Optional*, int): The initial brightness of the display, for AMOLED displays only. This should be a value from 0 to 255, and defaults to 0xD0.
- **color_order** (*Optional*): Should be one of `bgr` (default) or `rgb`. This specifies the order of the color channels in the display panel. The default is `bgr` for most displays, but some displays may require `rgb`. It does not affect the color order of the display buffer, which is always RGB.
- **dimensions** (*Optional*): Dimensions of the screen, specified either as *width* **x** *height* (e.g `320x240`  ) or with separate config keys. If not provided the dimensions will be determined by the model selected. This is required for the `CUSTOM` model, and is optional for other models. The dimensions are specified in pixels, and the width and height must be greater than 0. The following keys are available:

  - **height** (**Required**, int): Specifies height of display in pixels.
  - **width** (**Required**, int): Specifies width of display.
  - **offset_width** (*Optional*, int): Specify an offset for the x-direction of the display, typically used when an LCD is smaller than the maximum supported by the driver chip. Default is 0
  - **offset_height** (*Optional*, int): Specify an offset for the y-direction of the display. Default is 0.

- **invert_colors** (*Optional*, boolean): Specifies whether the display colors should be inverted. Options are `true` or `false`. Defaults to `false`.
- **rotation** (*Optional*): Rotate the display presentation in software. Choose one of `0°`, `90°`, `180°`, or `270°`. If the driver chip supports hardware rotation for the given orientation this will be translated to the appropriate hardware command. If hardware rotation is not supported, the display will be rotated in software.
- **transform** (*Optional*): If `rotation` is not sufficient, use this to transform the display. If this option is specified, then the `dimensions` option must also be provided. The value can either be the string `disabled` to disable hardware transform, or a dictionary. Options are:
  This option should not be used with `rotation`. For the `CUSTOM` model, use `transform: disabled`
  if the display does not support it, which will prevent a `rotation` being translated to a hardware transform.

  - **swap_xy** (**Required**, boolean): If true, exchange the x and y axes.
  - **mirror_x** (**Required**, boolean): If true, mirror the x axis.
  - **mirror_y** (**Required**, boolean): If true, mirror the y axis.

- **color_depth** (*Optional*): The color depth of the display buffer, expressed in bits. Options are `16` (default) and `8`. 8 bit depth will result in only 256 possible colors and should be used only if the microcontroller has limited memory. The driver will convert the 8 bit color to the display chip's required format.
- **buffer_size** (*Optional*): The percentage of screen size to allocate buffer memory. The default is `100%` when PSRAM is configured, and otherwise will be calculated to
  achieve a buffer size less than 20K bytes. See the discussion below about buffer sizes.

### Advanced options

- **init_sequence** (*Optional*): Allows custom initialisation sequences to be added. See below for more information.
- **pixel_mode** (*Optional*): Select the interface mode for the display driver. Options are `16bit` (default) and `18bit`. Most displays require 16 bit mode, and it is preferred unless the display requires 18 bit mode.
- **spi_16** (*Optional*): Set to `true` on boards where single bit SPI is used but drives the display in parallel via a 16 bit shift register.
- **data_rate** (*Optional*): The SPI data rate. Defaults to 10MHz but board presets may override this.
- **spi_mode** (*Optional*): The SPI mode. Options are `MODE0`, `MODE1`, `MODE2`, and `MODE3`. Defaults to `MODE0` for single bit SPI and `MODE3` for octal SPI (parallel bus.)
- **draw_rounding** (*Optional*): The rounding factor for drawing operations. Defaults to 2. Some chips require a higher value to avoid display artifacts. Must be a power of 2.
- **use_axis_flips** (*Optional*): If true, the driver will use alternate bits in the MADCTL register to implement x and y mirroring. Defaults to false.
- **byte_order** (*Optional*): The byte order of the display buffer. Options are `big_endian` (default) and `little_endian`. This affects the byte order for the buffer when
  using 16 bit color depth. The default is appropriate for the majority of displays.

**Note:** The maximum achievable data rate will depend on the chip type (e.g. ESP32 vs ESP32-S3) the pins used (on ESP32 using the default SPI pins allows higher rates) and the connection type (on-board connections will support higher rates than long cables or DuPont wires.) If in doubt, start with a low speed and test higher rates to find what works. A MISO pin should preferably not be specified, as this will limit the maximum rate in some circumstances, and is not required if the SPI bus is used only for the display.

## Buffer Size

The display driver writes data from a buffer to the display chip. When using {{< docref "/components/lvgl" >}} no buffer is required in the display driver itself, as LVGL will
allocate and use its own buffer.
When instead using a lambda function to update the display (and not LVGL), a buffer is required to be allocated by the display driver.
The size of this buffer is determined by the `buffer_size` option. The default is `100%` when PSRAM is configured, and otherwise will be calculated to
achieve a buffer size less than 20K bytes. For example a display of size 320x240 will have a buffer size of `320 * 240 * 2` bytes (for RGB565) = `153600` bytes.
If the buffer size is set to `50%`, then the buffer would occupy `76800` bytes. If 8 bit color depth is used, then each pixel occupies only 1 byte.

### Effect on Drawing Performance

The buffer size is a trade-off between the size of the buffer and the performance of the display driver. A larger buffer size will provide better performance,
but on boards with limited memory, a smaller buffer size may be required to avoid running out of memory. When using a buffer less than 100%, the driver will
call the drawing lambda multiple times to draw each chunk of the display. For example, with a 25% buffer size, the driver will call the drawing lambda four times to draw the display.
This has an effect on performance, and should be considered when setting the buffer size, but it is also important that the drawing lambda does not have
side effects - this should be avoided in any case, but becomes more critical when using a buffer less than 100%.

## Additional inititialisation sequences

The `init_sequence` option allows additional configuration of the driver chip. Provided commands will be sent to the
driver chip in addition to, and after the chosen model's pre-defined commands. It requires a list of byte sequences:

```yaml
init_sequence:
  - [ 0xD0, 0x07, 0x42, 0x18]
  - delay 10ms
  - [ 0xD1, 0x00, 0x07, 0x10]
```

Each entry represents a single-byte command followed by zero or more data bytes. Delays can be inserted with the `delay` keyword followed by a time in milliseconds. The delay is not precise, but will be at least the specified time.
If converting from other code, make sure the length byte, if present, is not copied as the length of each command sequence is determined by the number of bytes in the list.

## CUSTOM model

The `CUSTOM` model selection is provided for otherwise unsupported displays, and requires both `dimensions:` and `init_sequence:` to be specified. There is no pre-defined init sequence.

## Using the `transform` options

In most cases, the `rotation` option will be sufficient to orient the display correctly. However, some displays may require additional transformations. The `transform` option allows for these transformations to be applied in any of 8 different
combinations. It may be necessary to experiment with different combinations to achieve the desired result. When using the `transform` option, the `rotation` option should not be set unless the display does not support axis-swapping.
If the `swap_xy` option is set, then the `dimensions` option is required, and the `width` and `height` values should be set to reflect the final screen dimensions after rotation.

```yaml
transform:
  swap_xy: true
  mirror_x: true
  mirror_y: false
dimensions:
  height: 480
  width: 320
```

When using the `CUSTOM` model with rotation, if the chip does not support hardware transform use `transform: disabled` to disable hardware transform and ensure software rotation.

## LCD Backlights

Many displays have an integrated backlight, which may need to be turned on for the display to show. This backlight is not controlled
by the driver, but can be controlled by a separate GPIO pin. Depending on the display, the backlight may be active high or active low, and may
be able to be dimmed using a {{< docref "/components/light/monochromatic" >}} with a {{< docref "/components/output/ledc" >}}. AMOLED displays do not have a backlight but
their brightness can be set using the `brightness` option. This may also be controlled by a lambda API call.

## Touchscreens

A touchscreen, if present, must be configured separately. See the {{< docref "/components/touchscreen" >}} documentation for more information.

## See Also

- {{< docref "index/" >}}
- {{< docref "/components/lvgl" >}}
- {{< apiref "mipi_spi/mipi_spi.h" "mipi_spi/mipi_spi.h" >}}
