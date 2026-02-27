# LILYGO T5 4.7" Plus – ESPHome Component

ESPHome component for the [LILYGO T5 4.7" Plus](https://www.lilygo.cc/products/t5-4-7-inch-e-paper-v2-3) e-paper display board.

## Hardware

| Feature | Detail |
|---|---|
| MCU | ESP32-S3 |
| Display | 4.7" E-Paper, 960×540 px, up to 16-level grayscale |
| Touch | GT911 capacitive touchscreen (I2C) |
| Battery | LiPo connector, voltage monitor on GPIO14 |
| Flash / PSRAM | 16 MB Flash (QIO), 8 MB OPI PSRAM @ 80 MHz |
| Framework | Arduino only (I2S/RMT display bus) |

---

## Getting Started

All board-level settings (ESP32-S3 variant, flash mode, PSRAM, framework) are bundled in `board_config.yaml`. Pull it via `packages` — no manual `esp32:` or `psram:` blocks needed.

**For local/fork use** (`!include`):

```yaml
packages:
  board: !include path/to/esphome/components/lilygo_t5_47_plus/board_config.yaml
```

**For the ESPHome web installer** (remote, no local checkout):

```yaml
packages:
  board: github://hbast/esphome/esphome/components/lilygo_t5_47_plus/board_config.yaml@feature_lilygo_t5_47_plus

external_components:
  - source:
      type: git
      url: https://github.com/hbast/esphome
      ref: feature_lilygo_t5_47_plus
    components:
      - lilygo_t5_47_plus
    refresh: 1h
```

---

## Minimal Working Example

```yaml
esphome:
  name: my-lilygo

packages:
  board: github://hbast/esphome/esphome/components/lilygo_t5_47_plus/board_config.yaml@feature_lilygo_t5_47_plus

external_components:
  - source:
      type: git
      url: https://github.com/hbast/esphome
      ref: feature_lilygo_t5_47_plus
    components: [lilygo_t5_47_plus]
    refresh: 1h

logger:
  hardware_uart: USB_SERIAL_JTAG

font:
  - file: "gfonts://Roboto"
    id: my_font
    size: 32

display:
  - platform: lilygo_t5_47_plus
    update_interval: 60s
    lambda: |-
      it.fill(COLOR_OFF);
      it.print(480, 270, id(my_font), TextAlign::CENTER, "Hello World!");
```

---

## Components

### Display

```yaml
display:
  - platform: lilygo_t5_47_plus
    id: my_display
    greyscale: false        # true = 16-level grayscale, false = binary (faster)
    update_interval: 60s
    lambda: |-
      it.fill(COLOR_OFF);
      it.print(10, 10, id(my_font), "Hello!");
```

| Option | Type | Default | Description |
|---|---|---|---|
| `greyscale` | bool | `false` | 16-level grayscale (`true`) or binary black/white (`false`) |
| `update_interval` | duration | `5s` | How often the display lambda runs and the screen refreshes |
| `lambda` | lambda | — | Drawing code; `it` is the display reference |
| `pages` | list | — | Multiple pages; mutually exclusive with `lambda` |

**Color note:** This E-Paper display uses an inverted color model when drawing:

| ESPHome constant | Effect on screen |
|---|---|
| `COLOR_OFF` (`0,0,0`) | **White** (paper white) |
| `COLOR_ON` (`255,255,255`) | **Black** |

Always use `it.fill(COLOR_OFF)` for a white background.

---

### Touchscreen

The I2C bus is configured automatically by `board_config.yaml` — no `i2c:` block needed.

```yaml
touchscreen:
  - platform: lilygo_t5_47_plus
    id: my_touch
    update_interval: 100ms
    transform:
      swap_xy: true   # required for landscape orientation
      mirror_y: true  # required for correct top/bottom mapping
    on_touch:
      - lambda: |-
          ESP_LOGI("touch", "TOUCH x=%d y=%d", touch.x, touch.y);
    on_release:
      - lambda: |-
          ESP_LOGI("touch", "RELEASE");
```

| Option | Type | Default | Description |
|---|---|---|---|
| `update_interval` | duration | `500ms` | Touch polling interval |
| `transform.swap_xy` | bool | `false` | Swap X/Y axes (needed for landscape) |
| `transform.mirror_x` | bool | `false` | Mirror X axis |
| `transform.mirror_y` | bool | `false` | Mirror Y axis (needed for this board) |

**Touch coordinates** are reported in display pixels (0–959 for X, 0–539 for Y) after transform.

**I2C address:** The GT911 uses address `0x5D` by default. If a soft-reset occurs without a full power cycle, the chip may fall back to `0x14` — the driver detects this automatically and logs a warning.

---

### Battery Sensor

Reads LiPo voltage via ADC on GPIO14 (1 MΩ/1 MΩ voltage divider). Reports voltage (V) and/or state of charge (%).

```yaml
sensor:
  - platform: lilygo_t5_47_plus
    update_interval: 60s
    min_voltage: 3.0   # 0%  (default: 3.0 V)
    max_voltage: 4.2   # 100% (default: 4.2 V)
    battery_voltage:
      name: "Battery Voltage"
    battery_level:
      name: "Battery Level"
```

| Option | Type | Default | Description |
|---|---|---|---|
| `battery_voltage` | sensor | — | Voltage in volts |
| `battery_level` | sensor | — | State of charge in percent |
| `min_voltage` | float | `3.0` | Voltage at 0% (discharge cutoff) |
| `max_voltage` | float | `4.2` | Voltage at 100% (fully charged) |

At least one of `battery_voltage` or `battery_level` must be present.

---

### Deep Sleep

The component integrates with ESPHome's standard [`deep_sleep`](https://esphome.io/components/deep_sleep.html) component. Before the ESP32-S3 enters deep sleep, the display's `on_safe_shutdown()` hook is called automatically, which powers off the EPD supply via `epd_poweroff_all()`. The e-paper image is retained without power.

```yaml
esphome:
  name: my-lilygo
  on_boot:
    then:
      - deep_sleep.prevent: deep_sleep_ctrl
      - component.update: my_display
      - delay: 10s
      - deep_sleep.allow: deep_sleep_ctrl

packages:
  board: github://esphome/esphome@dev/esphome/components/lilygo_t5_47_plus/board_config.yaml

logger:
  hardware_uart: USB_SERIAL_JTAG

font:
  - file: "gfonts://Roboto"
    id: my_font
    size: 32

display:
  - platform: lilygo_t5_47_plus
    id: my_display
    update_interval: never
    lambda: |-
      it.fill(COLOR_OFF);
      it.print(480, 270, id(my_font), TextAlign::CENTER, "Sleeping...");

deep_sleep:
  id: deep_sleep_ctrl
  run_duration: 30s
  sleep_duration: 5min
  wakeup_pin:
    number: GPIO21   # BUTTON_1 — active LOW, internal pull-up
    inverted: true
    mode:
      input: true
      pullup: true
```

**Available wakeup sources on this board:**

| GPIO | Function | Works as wakeup pin? |
|---|---|---|
| GPIO21 | BUTTON_1 | ✅ Yes |
| GPIO47 | TOUCH_INT (GT911) | ❌ No |

> **GPIO47 (TOUCH_INT) does not work as a deep sleep wakeup pin.** The GT911 touch controller is powered from the same supply as the e-paper panel, which is off during deep sleep. It does not generate a valid wakeup signal. Use the physical button on GPIO21 instead.

---

## Full Example (display + touch + battery)

```yaml
esphome:
  name: my-lilygo
  on_boot:
    priority: -100
    then:
      - component.update: my_display

packages:
  board: github://hbast/esphome/esphome/components/lilygo_t5_47_plus/board_config.yaml@feature_lilygo_t5_47_plus

external_components:
  - source:
      type: git
      url: https://github.com/hbast/esphome
      ref: feature_lilygo_t5_47_plus
    components: [lilygo_t5_47_plus]
    refresh: 1h

logger:
  hardware_uart: USB_SERIAL_JTAG

font:
  - file: "gfonts://Roboto"
    id: my_font
    size: 32

sensor:
  - platform: lilygo_t5_47_plus
    update_interval: 60s
    battery_voltage:
      name: "Battery Voltage"
    battery_level:
      name: "Battery Level"

touchscreen:
  - platform: lilygo_t5_47_plus
    id: my_touch
    update_interval: 100ms
    transform:
      swap_xy: true
      mirror_y: true
    on_touch:
      - lambda: |-
          ESP_LOGI("touch", "TOUCH x=%d y=%d", touch.x, touch.y);
    on_release:
      - lambda: |-
          ESP_LOGI("touch", "RELEASE");

display:
  - platform: lilygo_t5_47_plus
    id: my_display
    greyscale: false
    update_interval: never
    lambda: |-
      it.fill(COLOR_OFF);
      it.print(480, 270, id(my_font), TextAlign::CENTER, "Touch the screen!");
```

---

## Notes

- **`update_interval: never` + `on_boot`**: E-paper displays refresh slowly (~2 s). Use `update_interval: never` and trigger a single refresh via `on_boot` or an automation to avoid unnecessary refreshes.
- **Full power cycle after flashing**: The GT911 touch controller latches its I2C address at power-on. After flashing, unplug and replug the USB cable rather than just pressing reset to ensure correct initialization.
- **Arduino framework only**: The IDF framework is not supported (the display bus uses Arduino I2S/RMT drivers). Deep sleep is fully supported via the standard `deep_sleep` component.
