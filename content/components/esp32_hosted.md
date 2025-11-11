---
description: "Instructions for setting up ESP32 hosted in ESPHome."
title: "ESP32 Hosted"
params:
  seo:
    description: Instructions for setting up ESP32 hosted in ESPHome.
    image: network-wifi.svg
---

ESP32 Hosted ([ESP-Hosted-MCU](https://github.com/espressif/esp-hosted-mcu)) is a
solution that allows you to use ESP32 modules as communication co-processors. This
solution provides wireless connectivity (Wi-Fi and Bluetooth) to the host module,
enabling it to communicate with other devices.

```yaml
# Example configuration entry
esp32_hosted:
  variant: ESP32C6
  reset_pin: GPIOXX
  cmd_pin: GPIOXX
  clk_pin: GPIOXX
  d0_pin: GPIOXX
  d1_pin: GPIOXX
  d2_pin: GPIOXX
  d3_pin: GPIOXX
  active_high: true

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
```

{{< anchor "esp32_hosted-configuration_variables" >}}

## Configuration variables

- **variant** (*Required*, string): The variant of the ESP32 co-processor that is used by the
  host. One of `ESP32`, `ESP32S2`, `ESP32S3`, `ESP32C2`, `ESP32C3` and `ESP32C6`.

- **clk_pin** (*Required*, [Pin](/guides/configuration-types#pin)): The SDIO clock pin.
- **cmd_pin** (*Required*, [Pin](/guides/configuration-types#pin)): The SDIO command pin.
- **d0_pin** (*Required*, [Pin](/guides/configuration-types#pin)): The SDIO d0 pin.
- **d1_pin** (*Required*, [Pin](/guides/configuration-types#pin)): The SDIO d1 pin.
- **d2_pin** (*Required*, [Pin](/guides/configuration-types#pin)): The SDIO d2 pin.
- **d3_pin** (*Required*, [Pin](/guides/configuration-types#pin)): The SDIO d3 pin.
- **slot** (*Optional*, int): The SDIO slot number. Defaults to 1.
- **reset_pin** (*Required*, [Pin](/guides/configuration-types#pin)): The reset pin of the co-processor.
- **active_high** (*Required*, boolean): If enabled, the co-processor is active when reset is
  high. If disabled, the co-processor is active when reset is low.

## Updating co-processor firmware

You can update the firmware on your ESP32 co-processor using the {{< docref "update/esp32_hosted/" >}}
platform. This allows you to deploy firmware updates to the co-processor without manually reflashing it.

## See Also

- {{< docref "wifi/" >}}
- {{< docref "network/" >}}
- {{< docref "ethernet/" >}}
- {{< docref "update/esp32_hosted/" >}}
- [ESP-Hosted-MCU](https://github.com/espressif/esp-hosted-mcu) by [Espressif Systems](https://www.espressif.com/)
