---
description: "Instructions for setting up chsc6x touch screen controller with ESPHome"
title: "chsc6x Touch Screen Controller"
params:
  seo:
    description: Instructions for setting up chsc6x touch screen controller with ESPHome
    image: chsc6x.png
---

The `chsc6x` touchscreen platform allows using the touch screen controllers based on the chsc6x chip with ESPHome.
The [I²C](/components/i2c) is required to be set up in your configuration for this touchscreen to work.

This controller is used in the Seeed Studio Round Display for XIAO with ILI9xxx display

{{< img src="chsc6x.png" alt="Image" caption="chsc6x touchscreen on Seeed Studio Round Display" width="50.0%" class="align-center" >}}

## Base Touchscreen Configuration

```yaml
# Example configuration entry
touchscreen:
  platform: chsc6x
  id: my_touchscreen
  display: my_display
  interrupt_pin: GPIO44
```

### Configuration variables

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually set the ID of this touchscreen.
- **interrupt_pin** (*Optional*, [Pin Schema](/guides/configuration-types#pin-schema)): The touch detection pin.

- All other options from [Touchscreen](/components/touchscreen#config-touchscreen).

### Sample config for the ESP32S3

```yaml
i2c:
  sda: GPIO5
  scl: GPIO6

spi:
  clk_pin: GPIO7
  mosi_pin: GPIO9

display:
  - platform: ili9xxx
    model: GC9A01A
    auto_clear_enabled: True
    invert_colors: True
    id: my_display
    cs_pin: GPIO2
    dc_pin: GPIO4

touchscreen:
  platform: chsc6x
  id: my_touchscreen
  display: my_display
  interrupt_pin: GPIO44
```

## See Also

- {{< apiref "chsc6x/chsc6x_touchscreen.h" "chsc6x/chsc6x_touchscreen.h" >}}
