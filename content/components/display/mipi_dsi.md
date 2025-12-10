---
description: "Details for the MIPI DSI display driver component in ESPHome"
title: "MIPI DSI Display Driver"
params:
  seo:
    description: Details for the MIPI DSI display driver component in ESPHome
    image: tab5.jpg
---

{{< anchor "mipi_dsi" >}}

## Introduction

This driver is for displays that use the MIPI DSI display interface available in the ESP32-P4.

{{< img src="tab5.jpg" alt="Image" caption="M5STACK-TAB5 with ESP32-P4" width="75.0%" class="align-center" >}}

## Background

The MIPI (Mobile Industry Processor Interface) Alliance publishes various hardware and software interface specifications
including the Display Serial Interface (DSI), which transfers pixel data over a high-speed serial bus to an LCD display.

The display panels controlled by the driver may be of various types, including TFT, IPS, and others. Each driver
chip and panel combination requires a specific set of initialisation commands, and standard initialisation sequences are provided for many common
boards and chips, but the driver is also designed to be customisable in YAML for other displays.

## Supported boards and driver chips

There are specific configurations for several ESP32 boards with integrated displays. For those boards
the predefined configuration will set the correct pins and dimensions for the display.

For custom displays, the driver can be configured with the correct pins and dimensions, and the driver chip can be
specified, or a custom init sequence can be provided.

### Driver chips

| Driver Chip | Typical Dimensions |
| ----------- | ------------------ |
| CUSTOM      | Customisable       |

### Boards with integrated displays

| Model                  | Manufacturer | Product Description                                                           |
| ---------------------- | ------------ | ----------------------------------------------------------------------------- |
| JC1060P470             | Guition      | <https://aliexpress.com/item/1005008328088576.html>                           |
| JC4880P443             | Guition      | <https://aliexpress.com/item/1005009618259341.html>                           |
| M5STACK-TAB5           | M5Stack      | <https://shop.m5stack.com/products/m5stack-tab5-iot-development-kit-esp32-p4> |
| WAVESHARE-P4-NANO-10.1 | Waveshare | <https://www.waveshare.com/esp32-p4-nano.htm?sku=29031> |
| WAVESHARE-P4-86-PANEL | Waveshare | <https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-4b.htm?sku=31570> |

## Configuration

```yaml
# Example minimal configuration entry
display:
    - platform: mipi_dsi
      model: WAVESHARE-P4-NANO-10.1
```

### Configuration options

All [graphical display configuration](/components/display#display-configuration) options are available, plus the following. For integrated display boards
most of the configuration will be set by default, but can be overridden if needed.

- **model** (**Required**): Chosen from the lists of supported chips and models above, or `CUSTOM` for custom displays.
- **reset_pin** (*Optional*, [Pin Schema](/guides/configuration-types#pin-schema)): The RESET pin, if required.
- **enable_pin** (*Optional*, [Pin Schema](/guides/configuration-types#pin-schema)): An optional pin to enable the display, if required. A list of pins can be provided for displays that require multiple enable pins. A full pin configuration may be provided
  to set the pin mode and inverted property. By default the pin will be driven high to enable the display.

- **color_order** (*Optional*): Should be one of `bgr` (default) or `rgb`. This specifies the order of the color channels in the display panel. The default is `bgr` for most displays, but some displays may require `rgb`. It does not affect the color order of the display buffer, which is always RGB.
- **dimensions** (*Optional*): Dimensions of the screen, specified either as *width* **x** *height* (e.g `320x240`  ) or with separate config keys. If not provided the dimensions will be determined by the model selected. This is required for the `CUSTOM` model, and is optional for other models. The dimensions are specified in pixels, and the width and height must be greater than 0. The following keys are available:

  - **height** (**Required**, int): Specifies height of display in pixels.
  - **width** (**Required**, int): Specifies width of display.
  - **offset_width** (*Optional*, int): Specify an offset for the x-direction of the display, typically used when an LCD is smaller than the maximum supported by the driver chip. Default is 0
  - **offset_height** (*Optional*, int): Specify an offset for the y-direction of the display. Default is 0.

- **invert_colors** (*Optional*, boolean): Specifies whether the display colors should be inverted. Options are `true` or `false`. Defaults to `false`.
- **rotation** (*Optional*): Rotate the display presentation in software. Choose one of `0°`, `90°`, `180°`, or `270°`. If the driver chip supports hardware rotation for the given orientation this will be translated to the appropriate hardware command. If hardware rotation is not supported, the display will be rotated in software.
- **transform** (*Optional*): If `rotation` is not sufficient, use this to transform the display. If this option is specified, then the `dimensions` option must also be provided. Options are:

  - **swap_xy** (**Required**, boolean): If true, exchange the x and y axes.
  - **mirror_x** (**Required**, boolean): If true, mirror the x axis.
  - **mirror_y** (**Required**, boolean): If true, mirror the y axis.

- **hsync_pulse_width** (*Optional*, int): The horizontal sync pulse width.
- **hsync_front_porch** (*Optional*, int): The horizontal front porch length.
- **hsync_back_porch** (*Optional*, int): The horizontal back porch length.
- **vsync_pulse_width** (*Optional*, int): The vertical sync pulse width.
- **vsync_front_porch** (*Optional*, int): The vertical front porch length.
- **vsync_back_porch** (*Optional*, int): The vertical back porch length.
- **pclk_frequency** (*Optional*): Set the pixel clock speed. Default is 40MHz.
- **pclk_inverted** (*Optional*, bool): If the pclk is active negative (default is True)
- **lanes** (*Optional*, int): Number of serial data lanes to use - 1 or 2. Default is 2.
- **lane_bit_rate** (*Optional*, int): The bit rate of the serial data lanes. No default unless a non-custom model is selected.

### Advanced options

- **init_sequence** (*Optional*): Allows custom initialisation sequences to be added. See below for more information.
- **pixel_mode** (*Optional*): Select the interface mode for the display driver. Options are `16bit` (default) and `24bit`.
- **color_depth** (*Optional*): The color depth of the display buffer, expressed in bits. Options are `16` (default) and `24`. Preferably should be the same as the `pixel_mode` option.
- **draw_rounding** (*Optional*): The rounding factor for drawing operations. Defaults to 2. Some chips require a higher value to avoid display artifacts. Must be a power of 2.
- **use_axis_flips** (*Optional*): If true, the driver will use alternate bits in the MADCTL register to implement x and y mirroring. Defaults to false.
- **byte_order** (*Optional*): The byte order of the display buffer. Options are `big_endian` and `little_endian` (default). This affects the byte order for the buffer when
  using 16 bit color depth. The default is appropriate for the majority of displays.

## Additional initialisation sequences

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

## LCD Backlights

Many displays have an integrated backlight, which may need to be turned on for the display to show. This backlight is not controlled
by the driver, but can be controlled by a separate GPIO pin. Depending on the display, the backlight may be active high or active low, and may
be able to be dimmed using a {{< docref "/components/light/monochromatic" >}} with a {{< docref "/components/output/ledc" >}}.

## Touchscreens

A touchscreen, if present, must be configured separately. See the {{< docref "/components/touchscreen" >}} documentation for more information.

## See Also

- {{< docref "index/" >}}
- {{< docref "/components/lvgl" >}}
- {{< apiref "mipi_dsi/mipi_dsi.h" "mipi_dsi/mipi_dsi.h" >}}
