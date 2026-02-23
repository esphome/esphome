# LILYGO T5 4.7" Plus E-Paper Display

ESPHome component for the [LILYGO T5 4.7" Plus](https://www.lilygo.cc/products/t5-4-7-inch-e-paper-v2-3) E-Paper display board (ESP32-S3).

## Hardware

- **MCU**: ESP32-S3
- **Display**: 4.7" E-Paper, 960×540 pixels, 16-level grayscale
- **Touch**: GT911 capacitive touchscreen
- **Battery**: LiPo connector with ADC voltage monitor (GPIO2, 1 MΩ/1 MΩ divider)
- **EPD interface**: I2S parallel bus + RMT pulse generation (Arduino framework required)

## Platform Constraints

- **Arduino framework only**: The I2S and RMT peripheral drivers use Arduino APIs.
- **ESP32-S3 only**: The pin assignments are fixed for this board.

## Quick Start

Use the included `board_config.yaml` package to set up the ESP32-S3 board with the correct framework and pin configuration:

```yaml
packages:
  board: github://esphome/esphome/esphome/components/lilygo_t5_47_plus/board_config.yaml

logger:

display:
  - platform: lilygo_t5_47_plus
    update_interval: 60s
    lambda: |-
      it.print(10, 10, id(my_font), "Hello World!");
```

## Components

### Display (`platform: lilygo_t5_47_plus`)

960×540 E-Paper display with optional 4-bit grayscale rendering.

```yaml
display:
  - platform: lilygo_t5_47_plus
    id: epaper
    greyscale: true        # true = 16-level grayscale, false = binary (default: false)
    update_interval: 60s
    lambda: |-
      it.fill(COLOR_OFF);
      it.print(10, 10, id(my_font), "Hello World!");
```

| Option | Type | Default | Description |
|---|---|---|---|
| `greyscale` | bool | `false` | Enable 4-bit grayscale rendering |
| `update_interval` | duration | `5s` | How often `lambda` is called |
| `lambda` | lambda | — | Drawing code |
| `pages` | list | — | Multiple pages (mutually exclusive with `lambda`) |

### Battery Sensor (`platform: lilygo_t5_47_plus`)

Reads battery voltage via ADC on GPIO2 (1 MΩ/1 MΩ voltage divider, 4.2 V full charge).

```yaml
sensor:
  - platform: lilygo_t5_47_plus
    battery_voltage:
      name: "Battery Voltage"
      filters:
        - sliding_window_moving_average:
            window_size: 10
    battery_level:
      name: "Battery Level"
      min_voltage: 3.3
      max_voltage: 4.2
```

### Touchscreen (`platform: lilygo_t5_47_plus`)

GT911 capacitive touchscreen (custom driver with 16-bit I2C register addressing and GPIO47 wake-up sequence).

```yaml
touchscreen:
  - platform: lilygo_t5_47_plus
    id: touch
    on_touch:
      - lambda: |-
          ESP_LOGI("touch", "Touch at (%d, %d)", touch.x, touch.y);
    on_release:
      - lambda: |-
          ESP_LOGI("touch", "Released");
```

## Driver Files

The EPD driver is embedded from [epdiy](https://github.com/vroland/epdiy) (LGPL-3.0), as further modified by [Xinyuan-LilyGO/LilyGo-EPD47](https://github.com/Xinyuan-LilyGO/LilyGo-EPD47) (GPL-3.0). All files carry SPDX attribution headers. Changes made for ESPHome:

- Port to ESP-IDF 5.x RMT driver API
- Port to ESP-IDF 5.x LCD clock source API
- Removal of zlib font decompression
- Removal of IDF version guards

## License

The ESPHome framework is licensed under GPLv3. The embedded EPD driver files are licensed under LGPL-3.0 (epdiy) and GPL-3.0 (LilyGo modifications), which are compatible with GPLv3. See the SPDX headers in each driver file for details.
