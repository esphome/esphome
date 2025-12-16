---
description: "Instructions for setting up addressable lights like NEOPIXEL on an ESP32 using the RMT peripheral."
title: "ESP32 RMT LED Strip"
params:
  seo:
    description: Instructions for setting up addressable lights like NEOPIXEL on an ESP32 using the RMT peripheral.
    image: color_lens.svg
---

This is a component using the ESP32 RMT peripheral to drive most addressable LED strips.

```yaml
light:
  - platform: esp32_rmt_led_strip
    rgb_order: GRB
    pin: GPIOXX
    num_leds: 30
    chipset: ws2812
    name: "My Light"
```

## Configuration variables

- **pin** (**Required**, [Pin](/guides/configuration-types#pin)): The pin for the data line of the light.
- **num_leds** (**Required**, int): The number of LEDs in the strip.
- **chipset** (**Required**, enum): The name of the chipset used; determines signal timing. Not required if
  [specifying the timings manually](#esp32-rmt-led-strip-manual_timings).

  - `WS2811`
  - `WS2812`
  - `SK6812`
  - `APA106`
  - `SM16703`

- **rgb_order** (**Required**, string): The RGB order of the strip.

  - `RGB`
  - `RBG`
  - `GRB`
  - `GBR`
  - `BGR`
  - `BRG`

- **is_rgbw** (*Optional*, boolean): Set to `true` if the strip is RGBW. Defaults to `false`.
- **is_wrgb** (*Optional*, boolean): Set to `true` if the strip is WRGB. Defaults to `false`.
- **max_refresh_rate** (*Optional*, [Time](/guides/configuration-types#time)): A time interval used to limit the number of commands a light
  can handle per second. For example, `16ms` will limit the light to a refresh rate of about 60Hz. Defaults to
  sending commands as quickly as changes are made to the lights.

- **use_psram** (*Optional*, boolean): Set to `false` to force internal RAM allocation even if you have the PSRAM
  component enabled. This can be useful if you're experiencing issues like flickering with your leds strip. Defaults to `true`.

- **rmt_symbols** (*Optional*, int): When `use_dma` is enabled, this sets the size of the driver's internal DMA
  buffer. When DMA is disabled, it specifies how much RMT memory is allocated to the component. RMT memory is shared
  across all components and should be allocated in multiples of the block size. On the `ESP32` and `ESP32-S2`
  variants, RMT memory is shared between RX and TX components. On other variants, RX and TX have dedicated RMT memory.

| ESP32 Variant | Available Memory | Block Size |
| ------------- | ---------------- | ---------- |
| ESP32         | 512 symbols      | 64 symbols |
| ESP32-C3      | 96 symbols       | 48 symbols |
| ESP32-C5 | 96 symbols | 48 symbols |
| ESP32-C6 | 96 symbols | 48 symbols |
| ESP32-C61 | 96 symbols | 48 symbols |
| ESP32-H2 | 96 symbols | 48 symbols |
| ESP32-P4 | 192 symbols | 48 symbols |
| ESP32-S2 | 256 symbols | 64 symbols |
| ESP32-S3 | 192 symbols | 48 symbols |

- **use_dma** (*Optional*, boolean): Enable DMA on variants that support it. If enabled `rmt_symbols` controls
  the DMA buffer size and can be set to a large value.

- All other options from [Light](/components/light#config-light).

{{< anchor "esp32-rmt-led-strip-manual_timings" >}}

### Manual Timings

These can be used if you know the timings and your chipset is not set above. If you have a new specific chipset,
please consider adding support to the codebase and add it to the list above.

- **bit0_high** (*Optional*, [Time](/guides/configuration-types#time)): The time to hold the data line high for a `0` bit.
- **bit0_low** (*Optional*, [Time](/guides/configuration-types#time)): The time to hold the data line low for a `0` bit.
- **bit1_high** (*Optional*, [Time](/guides/configuration-types#time)): The time to hold the data line high for a `1` bit.
- **bit1_low** (*Optional*, [Time](/guides/configuration-types#time)): The time to hold the data line low for a `1` bit.
- **reset_high** (*Optional*, [Time](/guides/configuration-types#time)): The time to hold the data line high after writing
  the state. Defaults to `0 us`.

- **reset_low** (*Optional*, [Time](/guides/configuration-types#time)): The time to hold the data line low after writing
  the state. Defaults to `0 us`.

## See Also

- {{< docref "/components/light" >}}
- {{< docref "/components/power_supply" >}}
- {{< apiref "esp32_rmt_led_strip/esp32_rmt_led_strip.h" "esp32_rmt_led_strip/esp32_rmt_led_strip.h" >}}
