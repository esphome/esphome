---
description: "Guide for migrating ESP32 devices from Arduino framework to ESP-IDF"
title: "ESP32 Arduino to ESP-IDF Migration Guide"
params:
  seo:
    description: Guide for migrating ESP32 devices from Arduino framework to ESP-IDF
    image: esp32.svg
---

Starting with ESPHome 2026.1.0, the default framework for ESP32 will change from Arduino to ESP-IDF. This guide will
help you migrate your existing configurations or make an informed choice about which framework to use.

> [!NOTE]
> The Arduino framework is built as an ESP-IDF component on top of ESP-IDF, providing Arduino API compatibility
> within the ESP-IDF build system. This means Arduino builds include both the ESP-IDF framework and the Arduino
> compatibility layer, resulting in longer build times, more flash usage, and more RAM usage compared to native ESP-IDF.

> [!NOTE]
> This change only affects ESP32, ESP32-S2, ESP32-S3, and ESP32-C3 variants.
> Newer variants (ESP32-C6, ESP32-H2, ESP32-P4, etc.) already default to ESP-IDF
> as they have limited or no Arduino support.

## Why the Change?

ESP-IDF (Espressif IoT Development Framework) is the official development framework for ESP32. It offers several
advantages:

- **Smaller Binaries**: Up to 40% reduction in binary size
- **Better Performance**: More optimized for ESP32 hardware
- **Custom Builds**: Firmware is built specifically for your device configuration
- **Active Development**: All ESPHome developers use and test with ESP-IDF
- **Latest Features**: New ESP32 features are available in ESP-IDF first

## Trade-offs

While ESP-IDF offers many benefits, there are some trade-offs to consider:

- **Component Compatibility**: Some components may need to be replaced with ESP-IDF compatible alternatives
- **Library Differences**: Arduino-specific libraries won't be available

## Making Your Choice

### Option 1: Migrate to ESP-IDF (Recommended)

Add the following to your ESP32 configuration:

```yaml
esp32:
  board: esp32dev  # Your board type
  framework:
    type: esp-idf
```

### Option 2: Stay with Arduino

If you prefer to continue using Arduino (which will remain supported), explicitly specify it:

```yaml
esp32:
  board: esp32dev  # Your board type
  framework:
    type: arduino
```

## Migration Steps

1. **Backup Your Configuration**: Always keep a backup of your working configuration before making changes.

1. **Check Component Compatibility**: When you compile with ESP-IDF, ESPHome will automatically notify you if any
   components are incompatible and suggest alternatives.

1. **Update Your Configuration**: Add the framework specification as shown above.

1. **Clean Build Files**: After changing frameworks, clean your build files:

   - **Using ESPHome CLI**

     ```shell
     esphome clean your-config.yaml
     ```

   - **Using ESPHome Dashboard**

     1. Click on the three-dot menu for your device
     1. Select "Clean Build Files"

1. **Compile and Test**: Compile your configuration and test thoroughly:

   - **Using ESPHome CLI**

     ```shell
     esphome compile your-config.yaml
     esphome upload your-config.yaml
     ```

   - **Using ESPHome Dashboard**

     1. Click "INSTALL" on your device
     1. Choose your preferred upload method (USB, OTA, etc.)
     1. The dashboard will automatically compile and upload

## Common Component Replacements

When migrating to ESP-IDF, you may need to replace some components. ESPHome will automatically suggest alternatives
when available:

**Components with ESP-IDF Alternatives:**

| Arduino Component                                              | ESP-IDF Alternative                                                          |
| -------------------------------------------------------------- | ---------------------------------------------------------------------------- |
| {{< docref "/components/sensor/bme680_bsec" "bme680_bsec" >}}  | {{< docref "/components/sensor/bme68x_bsec2" "bme68x_bsec2" >}}              |
| {{< docref "/components/light/fastled" "fastled_clockless" >}} | {{< docref "/components/light/esp32_rmt_led_strip" "esp32_rmt_led_strip" >}} |
| {{< docref "/components/light/fastled" "fastled_spi" >}}       | {{< docref "/components/light/spi_led_strip" "spi_led_strip" >}}             |
| {{< docref "/components/light/neopixelbus" "neopixelbus" >}}   | {{< docref "/components/light/esp32_rmt_led_strip" "esp32_rmt_led_strip" >}} |

**Arduino-Only Components:**

The following components currently require Arduino framework and don't have ESP-IDF alternatives or native ESP-IDF support yet:

- {{< docref "/components/output/ac_dimmer" "ac_dimmer" >}} - AC dimmer control
- {{< docref "/components/sensor/dsmr" "dsmr" >}} - Dutch Smart Meter integration
- {{< docref "/components/climate/climate_ir" "heatpumpir" >}} - IR-based heat pump control
- {{< docref "/components/climate/midea" "midea" >}} - Midea air conditioner control
- {{< docref "/components/light/index" "WLED Effect" >}} - WLED UDP Realtime Control integration

If you need these components, you will need to continue using the Arduino framework.

> [!NOTE]
> Component compatibility is constantly improving. Check the component documentation
> or try compiling with ESP-IDF to see if alternatives have become available.

## Troubleshooting

### Compilation Errors

If you encounter compilation errors after switching to ESP-IDF:

1. Check the error message for component compatibility issues
1. Look for suggested alternatives in the error output
1. Clean your build files and try again
1. Check the component documentation for ESP-IDF specific notes

### Build Time

ESP-IDF compilation is significantly faster than Arduino:

- **ESP-IDF is 2-3x faster** than Arduino framework
- On modern desktop systems: ESP-IDF saves 30-60 seconds per build
- On Raspberry Pi 5: ESP-IDF saves 2-4 minutes per build
- On Raspberry Pi 4 or older: ESP-IDF saves 6-10 minutes or more per build
- Subsequent builds maintain the same relative performance advantage

The faster build times are due to ESP-IDF's optimized build system and the elimination of the Arduino compatibility layer overhead.

### Performance Considerations

ESP-IDF generally offers better performance, but:

- Initial boot time might be slightly different
- Some timing-sensitive operations may need adjustment
- WiFi and Bluetooth behavior might have subtle differences

## Frequently Asked Questions

**Q: Will Arduino framework be removed?**
   A: No, Arduino framework will continue to be supported. Only the default is changing.

**Q: Do I have to migrate immediately?**
   A: No, but you should explicitly specify your framework choice to avoid the automatic change in 2026.1.0.

**Q: Can I switch back to Arduino if ESP-IDF doesn't work for me?**
   A: Yes, you can switch between frameworks at any time by changing your configuration.

**Q: Will my existing devices be affected?**
   A: Only when you recompile. Devices already running will continue to work as before.

**Q: How do I know which framework my device is currently using?**
   A: Check your device's log output during boot, or look at your configuration file.

## Getting Help

If you encounter issues during migration:

1. Check the [ESPHome Discord](https://discord.gg/KhAMKrd) for community support
1. Review component-specific documentation
1. Search existing [GitHub issues](https://github.com/esphome/esphome/issues)
1. Create a new issue if you find a bug

Remember, the migration is optional, and both frameworks will continue to be supported. Choose the option that
best fits your needs!

## See Also

- {{< docref "/components/esp32" >}}
- {{< docref "/guides/faq" >}}
- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/)
- [Arduino-ESP32 Documentation](https://docs.espressif.com/projects/arduino-esp32/)
