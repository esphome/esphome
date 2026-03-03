# Common Bus Configurations for Component Tests

This directory contains standardized bus configurations (I2C, SPI, UART, Modbus, BLE) that component tests use, enabling multiple components to be tested together with intelligent grouping.

## Purpose

These common configs allow multiple components to **share a single bus**, dramatically reducing CI time by compiling multiple compatible components together. Components with identical bus configurations are automatically grouped and tested together.

## Structure

```
common/
├── i2c/              # Standard I2C (50kHz)
│   ├── esp32-idf.yaml
│   ├── esp32-ard.yaml
│   ├── esp32-c3-idf.yaml
│   ├── esp32-c3-ard.yaml
│   ├── esp32-s2-idf.yaml
│   ├── esp32-s2-ard.yaml
│   ├── esp32-s3-idf.yaml
│   ├── esp32-s3-ard.yaml
│   ├── esp8266-ard.yaml
│   ├── rp2040-ard.yaml
│   └── bk72xx-ard.yaml
├── i2c_low_freq/     # Low frequency I2C (10kHz)
│   └── (same platform variants)
├── spi/
│   └── (same platform variants)
├── uart/
│   ├── esp32-idf.yaml
│   ├── esp32-c3-idf.yaml
│   ├── esp8266-ard.yaml
│   └── rp2040-ard.yaml
├── modbus/           # Modbus (includes uart via packages)
│   ├── esp32-idf.yaml
│   ├── esp32-c3-idf.yaml
│   ├── esp8266-ard.yaml
│   └── rp2040-ard.yaml
└── ble/
    ├── esp32-idf.yaml
    ├── esp32-ard.yaml
    └── esp32-c3-idf.yaml
```

## How It Works

### Component Test Structure
Each component test includes the common bus config:

```yaml
# tests/components/bh1750/test.esp32-idf.yaml
packages:
  i2c: !include ../../test_build_components/common/i2c/esp32-idf.yaml

<<: !include common.yaml
```

The common config provides:
- Standardized pin assignments
- A shared bus instance (`i2c_bus`, `spi_bus`, `uart_bus`, `modbus_bus`, etc.)

The component's `common.yaml` references the shared bus:
```yaml
# tests/components/bh1750/common.yaml
sensor:
  - platform: bh1750
    i2c_id: i2c_bus
    name: Living Room Brightness
    address: 0x23
```

### Intelligent Grouping (Implemented)
Components with identical bus configurations are automatically grouped and tested together:

```yaml
# Auto-generated merged config (created by test_build_components.py)
packages:
  i2c: !include ../../test_build_components/common/i2c/esp32-idf.yaml

sensor:
  - platform: bme280_i2c
    i2c_id: i2c_bus
    temperature:
      name: BME280 Temperature
  - platform: bh1750
    i2c_id: i2c_bus
    name: BH1750 Illuminance
  - platform: sht3xd
    i2c_id: i2c_bus
    temperature:
      name: SHT3xD Temperature
```

**Result**: 3 components compile in one test instead of 3 separate tests!

### Package Dependencies
Some packages include other packages to avoid duplication:

```yaml
# tests/test_build_components/common/modbus/esp32-idf.yaml
packages:
  uart: !include ../uart/esp32-idf.yaml  # Modbus requires UART

substitutions:
  flow_control_pin: GPIO4

modbus:
  - id: modbus_bus
    uart_id: uart_bus
    flow_control_pin: ${flow_control_pin}
```

Components using `modbus` packages automatically get `uart` as well.

## Pin Allocations

### I2C (Standard - 50kHz)
- **ESP32 IDF**: SCL=GPIO16, SDA=GPIO17
- **ESP32 Arduino**: SCL=GPIO22, SDA=GPIO21
- **ESP32-C3**: SCL=GPIO5, SDA=GPIO4
- **ESP32-S2/S3**: SCL=GPIO9, SDA=GPIO8
- **ESP8266**: SCL=GPIO5, SDA=GPIO4
- **RP2040**: SCL=GPIO5, SDA=GPIO4
- **BK72xx**: SCL=P20, SDA=P21

### I2C Low Frequency (10kHz)
Same pin allocations as standard I2C, but with 10kHz frequency for components requiring slower speeds.

### SPI
- **ESP32**: CLK=GPIO18, MOSI=GPIO23, MISO=GPIO19
- **ESP32-C3**: CLK=GPIO6, MOSI=GPIO7, MISO=GPIO5
- **ESP32-S2**: CLK=GPIO36, MOSI=GPIO35, MISO=GPIO37
- **ESP32-S3**: CLK=GPIO40, MOSI=GPIO6, MISO=GPIO41
- **ESP8266**: CLK=GPIO14, MOSI=GPIO13, MISO=GPIO12
- **RP2040**: CLK=GPIO18, MOSI=GPIO19, MISO=GPIO16
- CS pins are component-specific (each SPI device needs unique CS)

### UART
- **ESP32 IDF**: TX=GPIO17, RX=GPIO16 (baud: 19200)
- **ESP32-C3 IDF**: TX=GPIO7, RX=GPIO6 (baud: 19200)
- **ESP8266**: TX=GPIO1, RX=GPIO3 (baud: 19200)
- **RP2040**: TX=GPIO0, RX=GPIO1 (baud: 19200)

### Modbus (includes UART)
Same UART pins as above, plus:
- **flow_control_pin**: GPIO4 (all platforms)

### BLE
- **ESP32**: Shared `esp32_ble_tracker` infrastructure
- Each component defines unique `ble_client` with different MAC addresses

## Benefits

1. **Shared bus = less duplication**
   - 200+ I2C components use common bus configs
   - 60+ SPI components use common bus configs
   - 80+ UART components use common bus configs
   - 6 Modbus components use common modbus configs (which include UART)

2. **Intelligent grouping reduces CI time**
   - Components with identical bus configs are automatically grouped
   - Typical reduction: 80-90% fewer builds
   - Example: 3 I2C components → 1 merged build (saves 2 builds)
   - CI runs 5 component batches in parallel (configurable via `max-parallel` in `.github/workflows/ci.yml`)

3. **Easier maintenance**
   - Change bus pins for a platform once, affects all component tests
   - Consistent pin assignments across all components
   - Centralized bus configuration

## Component Compatibility

### Groupable Components
Components are automatically grouped when they have:
- Identical bus package references (e.g., all use `i2c/esp32-idf.yaml`)
- No local file references (`$component_dir`)
- No `!extend` or `!remove` directives
- Proper bus ID references (`i2c_id: i2c_bus`, `spi_id: spi_bus`, etc.)

### Non-Groupable Components
Components tested individually when they:
- Use different bus frequencies (e.g., `i2c` vs `i2c_low_freq`)
- Reference local files with `$component_dir`
- Are platform components (abstract base classes with `IS_PLATFORM_COMPONENT = True`)
- Define buses directly (not migrated to packages yet)
- Are in `ISOLATED_COMPONENTS` list (known build conflicts)

### Bus Variants
- **i2c** (50kHz): Standard I2C frequency for most sensors
- **i2c_low_freq** (10kHz): For sensors requiring slower I2C speeds
- **modbus**: Includes UART via package dependencies

### Making Components Groupable

**WARNING**: Using `!extend` or `!remove` directives in component test files prevents automatic component grouping in CI, making builds slower.

When platform-specific parameters are needed, inline the full configuration rather than using `!extend` or `!remove`. This allows the component to be grouped with other compatible components, reducing CI build time.

**Note**: Some components legitimately require `!extend` or `!remove` for platform-specific features (e.g., `adc` removing ESP32-only `attenuation` parameter on ESP8266). These are correctly identified as non-groupable.

## Testing Components

### Testing Individual Components
Test specific components using `test_build_components`:
```bash
# Test a single component
./script/test_build_components -c bme280_i2c -t esp32-idf -e config

# Test multiple components
./script/test_build_components -c bme280_i2c,bh1750,sht3xd -t esp32-idf -e compile
```

### Testing All Components Together
To verify that all components can be tested together without ID conflicts or configuration issues:
```bash
./script/test_component_grouping.py -e config --all
```

This tests all components in a single build to catch conflicts that might not appear when testing components individually. This is useful for:
- Detecting ID conflicts between components
- Validating that components can coexist in the same configuration
- Ensuring proper `i2c_id`, `spi_id`, `uart_id` specifications

Use `-e config` for fast configuration validation, or `-e compile` for full compilation testing.

### Testing Component Groups
Test specific groups of components by bus signature:
```bash
# Test all I2C components together
./script/test_component_grouping.py -s i2c -e config

# Test with custom group sizes
./script/test_component_grouping.py --min-size 5 --max-size 20 --max-groups 3
```

## Implementation Details

### Scripts
- `script/analyze_component_buses.py`: Analyzes components to detect bus usage and grouping compatibility
- `script/merge_component_configs.py`: Merges multiple component configs into a single test file
- `script/test_build_components.py`: Main test runner with intelligent grouping
- `script/test_component_grouping.py`: Test component groups or all components together
- `script/split_components_for_ci.py`: Splits components into batches for parallel CI execution

### Configuration
- `.github/workflows/ci.yml`: CI workflow with `max-parallel: 5` for component testing
- Package dependencies defined in `PACKAGE_DEPENDENCIES` (e.g., modbus → uart)
- Base bus components excluded from migration warnings: `i2c`, `spi`, `uart`, `modbus`, `canbus`
