---
description: "Configuration for the ESP32 PSRAM platform for ESPHome."
title: "PSRAM"
params:
  seo:
    description: Configuration for the ESP32 PSRAM platform for ESPHome.
    image: psram.svg
---

This component enables and configures PSRAM if/when available on ESP32 modules/boards.
Some components require PSRAM, while others may use it for optional features. In any case
it is required to explicitly configured - this is a change from previous versions of ESPHome.

PSRAM is not available with platforms other than ESP32.

```yaml
# Example configuration entry
psram:
  mode: octal
  speed: 80MHz
```

## Configuration variables

- **mode** (*Optional*): Defines the operating mode the PSRAM should utilize. One of `quad`, `octal` or `hex`.
  Defaults to `quad` for ESP32, ESP32-C5, ESP32-S2 and `hex` for ESP32-P4. ESP32-S3 has no default and *requires* this option to be set.
  See notes below.
- **speed** (*Optional*, int): The speed at which the PSRAM should operate. One of `40MHz` (default), `80MHz` or `120MHz`.
- **enable_ecc** (*Optional*, bool): For octal mode, enable ECC (Error Correction Code) for the PSRAM (default is off.)
  ECC is a method of detecting and correcting single-bit errors in memory. It will reduce the available PSRAM size and speed by
  1/16th, but also increases the rated temperature range of some ESP32 modules.

- **disabled** (*Optional*, bool): Don't try to initialize the PSRAM. This is needed if one of the configured components autoloads psram
  but the ESP32 module doesn't have PSRAM and you need to use one of the PSRAM control lines for something else. e.g. ethernet. Defaults to `false`.

- **ignore_not_found** (*Optional*, bool): When ``true`` (default), the firmware ignores PSRAM initialisation failures and continues to boot.
  When ``false``, other components can configure larger WiFi buffers for faster data transfer, but **PSRAM must be available or the device will
  fail to boot.**

## Modes

The ESP32, ESP32-C5 and ESP32-S2 PSRAM is only available in `quad` mode, and ESP32-P4 only supports `hex` mode. These are the defaults
when using those variants. For ESP32-S3, the `mode` option is required and must be set to `quad` or `octal`.
Typically on ESP32-S3 modules, a 2MB PSRAM will use quad mode, while 8 or 16MB will use octal mode, but check
the datasheet for the module you are using to be sure.

> [!WARNING]
> If you choose the wrong mode for your board, the PSRAM will not work.

## Notes

- Not all ESP32 modules have PSRAM available. If you are unsure, consult the datasheet of your module.
- Not all modules support all modes and speeds.
- 120MHz is not available with octal mode, unless using ESP-IDF and the `enable_idf_experimental_features` is enabled
  in the ESP-IDF platform [Advanced Configuration](/components/esp32#esp32-advanced_configuration).
- Configuring an unsupported speed will usually result in the PSRAM running at the default speed.

## See Also
