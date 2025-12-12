---
description: "Instructions for using the ESP32 Hosted update platform to manage co-processor firmware updates."
title: "ESP32 Hosted Co-processor Update"
params:
  seo:
    description: Instructions for using the ESP32 Hosted update platform to manage co-processor firmware updates.
    image: system-update.svg
---

This platform allows you to update the firmware of an ESP32 co-processor connected via the
{{< docref "/components/esp32_hosted" >}} component. The firmware binary is embedded into
your device's flash at compile time and can be deployed to the co-processor on demand.

The component automatically detects the current co-processor firmware version and compares it to the
version embedded in your device. If the versions differ, an update becomes available in Home Assistant
or through the ESPHome API.

```yaml
# Example configuration entry
# Note: Host device must be ESP32-H2 or ESP32-P4
esp32_hosted:
  variant: ESP32C6  # Co-processor variant
  reset_pin: GPIOXX
  cmd_pin: GPIOXX
  clk_pin: GPIOXX
  d0_pin: GPIOXX
  d1_pin: GPIOXX
  d2_pin: GPIOXX
  d3_pin: GPIOXX
  active_high: true

update:
  - platform: esp32_hosted
    path: coprocessor-firmware.bin
    sha256: 1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef
```

{{< anchor "update_esp32_hosted-configuration_variables" >}}

## Configuration variables

- **path** (**Required**, string): Path to the co-processor firmware binary file (`.bin`).
  The path is relative to your ESPHome configuration file.

- **sha256** (**Required**, string): SHA256 hash of the firmware binary file. This is used to verify
  the integrity of the firmware both at compile time and at runtime before flashing to the co-processor.

- All other options from [Update](/components/update#config-update).

## Platform requirements

This update platform requires:

- **Host device** (running ESPHome): `ESP32-H2` or `ESP32-P4`
- **Co-processor** (being updated): Any ESP32 variant supported by ESP-Hosted (e.g., `ESP32-C6` as shown in the example)

The host device must have sufficient flash space to store the co-processor firmware binary.

## Obtaining co-processor firmware

To build firmware for your ESP32 co-processor, refer to the
[ESP IDF Get Started](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html). The
firmware should be built using the ESP-IDF framework and the resulting `.bin` file should be placed in your ESPHome
configuration directory.

```sh
# Build instructions for IDF 5.5.1 and ESP Hosted 2.7.0
git clone -b v5.5.1 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32c6
source export.sh  # for Linux/macOS
export.bat        # for Windows
cd ..
idf.py create-project-from-example "espressif/esp_hosted^2.7.0:slave"
cd slave/
idf.py set-target esp32c6
idf.py build
```

After building the firmware, copy it to your ESPHome configuration directory and generate its SHA256 hash:

```sh
# Copy the firmware to your ESPHome config directory
cp build/network_adapter.bin /path/to/your/esphome/config/coprocessor-firmware.bin

# Generate SHA256 hash (Linux/macOS)
sha256sum /path/to/your/esphome/config/coprocessor-firmware.bin

# Generate SHA256 hash (Windows PowerShell)
Get-FileHash -Algorithm SHA256 coprocessor-firmware.bin

# Generate SHA256 hash (Windows Command Prompt with certutil)
certutil -hashfile coprocessor-firmware.bin SHA256
```

Use the generated hash in your `sha256` configuration parameter.

## See Also

- {{< docref "/components/esp32_hosted" >}}
- {{< docref "/components/update" >}}
- [ESP-Hosted-MCU](https://github.com/espressif/esp-hosted-mcu) by [Espressif Systems](https://www.espressif.com/)
- {{< apiref "esp32_hosted/update/esp32_hosted_update.h" "esp32_hosted/update/esp32_hosted_update.h" >}}
