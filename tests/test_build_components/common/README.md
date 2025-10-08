# Common Bus Configurations for Component Tests

This directory contains standardized bus configurations (I2C, SPI, UART, BLE) that component tests use, enabling multiple components to be tested together in Phase 2.

## Purpose

These common configs allow multiple components to **share a single bus**, dramatically reducing CI time from ~12 hours to ~2-3 hours by compiling multiple compatible components together.

## Structure

```
common/
├── i2c/
│   ├── esp32-idf.yaml
│   ├── esp32-ard.yaml
│   ├── esp32-c3-idf.yaml
│   ├── esp32-c3-ard.yaml
│   ├── esp8266-ard.yaml
│   └── rp2040-ard.yaml
├── spi/
│   └── esp32-idf.yaml
├── uart/
│   └── esp32-idf.yaml
└── ble/
    ├── esp32-idf.yaml
    ├── esp32-ard.yaml
    └── esp32-c3-idf.yaml
```

## How It Works

### Current State (Phase 1)
Each component test includes the common bus config:

```yaml
# tests/components/bh1750/test.esp32-idf.yaml
packages:
  i2c: !include ../../test_build_components/common/i2c/esp32-idf.yaml

<<: !include common.yaml
```

The common config provides:
- Standardized pin assignments
- A shared bus instance (`i2c_bus`, `spi_bus`, etc.)

The component's `common.yaml` just defines the sensor:
```yaml
# tests/components/bh1750/common.yaml
sensor:
  - platform: bh1750
    name: Living Room Brightness
    address: 0x23
```

The component **auto-detects** the shared bus - no need to specify `i2c_id`.

### Future State (Phase 2)
Create merged test YAMLs combining multiple compatible components:

```yaml
# tests/merged_components/i2c_sensors_group_1.esp32-idf.yaml
esphome:
  name: test_i2c_group_1

esp32:
  board: nodemcu-32s
  framework:
    type: esp-idf

packages:
  # Shared I2C bus
  i2c: !include ../test_build_components/common/i2c/esp32-idf.yaml

  # Multiple components sharing the same bus
  component_bme280: !include ../components/bme280_i2c/common.yaml
  component_bh1750: !include ../components/bh1750/common.yaml
  component_sht3xd: !include ../components/sht3xd/common.yaml
  component_ags10: !include ../components/ags10/common.yaml  # Different I2C address
  component_aht10: !include ../components/aht10/common.yaml
```

**Result**: 5 components compile in one test instead of 5 separate tests!

## Pin Allocations

### I2C
- **ESP32 IDF**: SCL=GPIO16, SDA=GPIO17
- **ESP32 Arduino**: SCL=GPIO22, SDA=GPIO21
- **ESP8266**: SCL=GPIO5, SDA=GPIO4
- **RP2040**: SCL=GPIO5, SDA=GPIO4

### SPI
- **ESP32 IDF**: CLK=GPIO18, MOSI=GPIO23, MISO=GPIO19
- CS pins are component-specific (each SPI device needs unique CS)

### UART
- **ESP32 IDF**: TX=GPIO17, RX=GPIO16
- Components define unique baud rates as needed

### BLE
- **ESP32**: Shared `esp32_ble_tracker` infrastructure
- Each component defines unique `ble_client` with different MAC addresses

## Benefits

1. **Shared bus = less duplication**
   - 171 I2C components removed duplicate bus definitions
   - 59 SPI components removed duplicate bus definitions
   - 75 UART components removed duplicate bus definitions

2. **Phase 2 merging enabled**
   - Compatible components can compile together
   - **Expected reduction: ~500 tests → ~50-100 tests (80-90%)**
   - **Expected CI time: 12 hours → 2-3 hours**

3. **Easier maintenance**
   - Change I2C pins for a platform once, affects all tests
   - Consistent pin assignments across all components

## Component Compatibility

Most components auto-detect and use the shared bus. Some edge cases:

- **Special frequency requirements**: Some components (like ags10) need custom I2C frequencies. These can't be merged with standard frequency components.
- **Multiple buses needed**: Rare, but some tests might need multiple UART ports.

These special cases will be handled individually in Phase 2.

## Migration Summary

- **11 common bus config files** created
- **1,459 component test files** migrated to use common configs
- **346 bus definitions** removed from component files
- **41 explicit bus ID references** removed (components now auto-detect)
- All sample tests validate successfully ✓
