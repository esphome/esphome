---
description: "Configuration for the ESP32 PSRAM platform for ESPHome."
title: "PSRAM"
params:
  seo:
    description: Configuration for the ESP32 PSRAM platform for ESPHome.
    image: psram.svg
---

This component enables and configures PSRAM if/when available on ESP32 modules/boards.
It is automatically loaded and enabled by components that require it.

PSRAM is only available on the ESP32.

```yaml
# Example configuration entry
psram:
  mode: octal
  speed: 80MHz
```

## Configuration variables

- **mode** (*Optional*): Defines the operating mode the PSRAM should utilize. One of `quad` (default) or `octal`.
- **speed** (*Optional*, int): The speed at which the PSRAM should operate. One of `40MHz` (default), `80MHz` or `120MHz`.
- **enable_ecc** (*Optional*, bool): For octal mode, enable ECC (Error Correction Code) for the PSRAM (default is off.)
  ECC is a method of detecting and correcting single-bit errors in memory. It will reduce the available PSRAM size and speed by
  1/16th, but also increases the rated temperature range of some ESP32 modules.

- **disabled** (*Optional*, bool): Don't try to initialize the PSRAM. This is needed if one of the configured components autoloads psram
  but the ESP32 module doesn't have PSRAM and you need to use one of the PSRAM control lines for something else. e.g. ethernet. Defaults to `false`.

## Restrictions

- Not all ESP32 modules have PSRAM available. If you are unsure, consult the datasheet of your module.
- Not all modules support all modes and speeds.
- 120MHz is not available with octal mode, unless using ESP-IDF and the `enable_idf_experimental_features` is enabled
  in the ESP-IDF platform [Advanced Configuration](#esp32-advanced_configuration).

- If you choose the wrong mode for your board, the PSRAM will not work.
- Configuring an unsupported speed will usually result in the PSRAM running at the default speed.
- Typically on ESP32-S3 modules, a 2MB PSRAM will use quad mode, while 8 or 16MB will use octal mode.

## See Also
