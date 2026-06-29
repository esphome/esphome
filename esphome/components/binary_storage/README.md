# Binary Storage Component

A unified interface for non-volatile binary storage devices (FRAM, EEPROM, Flash, MRAM) in ESPHome.

## Overview

The `binary_storage` component provides a consistent API for accessing various types of non-volatile memory devices over I2C, SPI, and OneWire buses. It supports both raw binary access and LittleFS filesystem mounting for structured data storage.

## Supported Devices

### I2C Devices

- **EEPROM**: AT24C series (AT24C32, AT24C64, AT24C128, AT24C256, etc.)
  - Write cycles: ~1 million per location
  - Write time: ~5ms per write
  - Best for: Infrequent configuration storage

- **FRAM**: Fujitsu MB85RC series, Cypress FM24 series
  - Write cycles: 100 trillion+ (unlimited for practical purposes)
  - Write time: <1μs (instant)
  - Best for: Frequent updates, counters, state storage

### SPI Devices

- **Flash**: Winbond W25Q series, Macronix MX25 series
  - Capacity: Up to 128MB
  - Write cycles: ~100,000 per sector
  - Requires erase before write
  - Best for: Large data storage, logs

- **FRAM**: Cypress FM25V series
  - Write cycles: 100 trillion+
  - Write time: Instant
  - Speed: Up to 40MHz SPI
  - Best for: High-speed logging

- **MRAM**: Everspin MR25H series
  - Write cycles: Unlimited
  - Write time: Instant
  - Non-volatile RAM characteristics
  - Best for: Mission-critical data

### OneWire Devices

- **EEPROM**: Dallas DS2431, DS28EC20
  - Low pin count (single data wire)
  - Best for: Simple configurations with minimal I/O

## Usage Modes

### Raw Mode
Direct binary access to storage device. Ideal for simple byte-level operations.

```yaml
binary_storage:
  - id: my_fram
    type: I2C_FRAM
    model: MB85RC256V
    address: 0x50
    mode: raw
```

### LittleFS Mode
Mount device as a LittleFS filesystem for file-based access (ESP-IDF only).

```yaml
binary_storage:
  - id: my_flash
    type: SPI_FLASH
    model: W25Q32
    cs_pin: GPIO5
    mode: littlefs
    mount_path: /data
    auto_format: true
```

### Both Mode
Combine raw access and filesystem mounting on the same device.

```yaml
binary_storage:
  - id: my_fram
    type: SPI_FRAM
    model: FM25V10
    cs_pin: GPIO15
    mode: both
    mount_path: /logs
```

## Configuration

### Common Parameters

```yaml
binary_storage:
  - id: storage_id              # Required: Unique ID
    type: DEVICE_TYPE           # Required: See device types below
    model: MODEL_STRING         # Optional: Auto-configures capacity
    capacity: SIZE              # Optional: Override auto-detected size
    mode: raw                   # Optional: raw, littlefs, or both (default: raw)
```

### Device Types

| Type | Aliases | Bus |
|------|---------|-----|
| `I2C_EEPROM` | `EEPROM` | I2C |
| `I2C_FRAM` | `FRAM` | I2C |
| `SPI_FLASH` | `FLASH` | SPI |
| `SPI_FRAM` | - | SPI |
| `SPI_MRAM` | `MRAM` | SPI |
| `ONEWIRE_EEPROM` | `ONEWIRE` | OneWire |

### I2C Configuration

```yaml
i2c:
  sda: GPIO21
  scl: GPIO22

binary_storage:
  - type: I2C_FRAM
    id: fram_storage
    model: MB85RC256V           # 32KB FRAM
    address: 0x50               # I2C address
    mode: raw
```

### SPI Configuration

```yaml
spi:
  clk_pin: GPIO18
  mosi_pin: GPIO23
  miso_pin: GPIO19

binary_storage:
  - type: SPI_FLASH
    id: flash_storage
    model: W25Q32               # 4MB Flash
    cs_pin: GPIO5
    mode: littlefs
    mount_path: /data
```

### OneWire Configuration

```yaml
binary_storage:
  - type: ONEWIRE_EEPROM
    id: onewire_storage
    model: DS2431              # 1KB EEPROM
    pin: GPIO4
    mode: raw
```

## Automation Actions

### Read Data

```yaml
on_boot:
  then:
    - binary_storage.read:
        id: fram_storage
        address: 0x0000
        length: 4
```

### Write Data

```yaml
on_...:
  then:
    - binary_storage.write:
        id: fram_storage
        address: 0x0000
        data: [0x01, 0x02, 0x03, 0x04]
```

### Write Single Byte

```yaml
on_...:
  then:
    - binary_storage.write_byte:
        id: fram_storage
        address: 0x0100
        value: 0xFF
```

### Write String

```yaml
on_...:
  then:
    - binary_storage.write_string:
        id: fram_storage
        address: 0x0000
        value: "Hello ESPHome!"
```

### Fill/Clear Storage

```yaml
on_...:
  then:
    - binary_storage.fill:
        id: fram_storage
        value: 0x00  # Clear entire storage to 0x00
```

## Lambda Access

### Raw Mode

```yaml
on_boot:
  then:
    - lambda: |-
        // Read boot counter from FRAM
        auto *fram = id(fram_storage);
        uint32_t boot_count = 0;
        fram->read(0x0000, (uint8_t*)&boot_count, sizeof(boot_count));
        boot_count++;
        fram->write(0x0000, (uint8_t*)&boot_count, sizeof(boot_count));
        ESP_LOGI("boot", "Boot count: %u", boot_count);
```

### LittleFS Mode

```yaml
on_boot:
  then:
    - lambda: |-
        // Write to file on mounted LittleFS
        FILE *f = fopen("/data/config.txt", "w");
        if (f) {
          fprintf(f, "Boot time: %lu\n", millis());
          fclose(f);
        }
```

## Device Characteristics

### Write Endurance Comparison

| Device Type | Write Cycles | Write Speed | Use Case |
|-------------|--------------|-------------|----------|
| EEPROM | ~1 million | ~5ms | Infrequent config |
| FRAM | 100 trillion+ | <1μs | Frequent updates |
| Flash | ~100K/sector | Fast (after erase) | Large storage |
| MRAM | Unlimited | <1μs | Mission-critical |

### Capacity Ranges

- **I2C EEPROM**: 256 bytes - 512KB
- **I2C FRAM**: 4KB - 256KB
- **SPI Flash**: 512KB - 128MB
- **SPI FRAM**: 8KB - 4MB
- **SPI MRAM**: 256KB - 16MB
- **OneWire EEPROM**: 128 bytes - 20KB

## Integration with Storage Host

When used with the `storage` component, binary storage devices automatically register:

- **Raw mode**: Device nodes like `/dev/fram0`, `/dev/eeprom0`
- **LittleFS mode**: Mount points like `/fram`, `/data`

This enables unified file system access across different storage types.

## Examples

### Boot Counter (FRAM)

```yaml
binary_storage:
  - id: fram
    type: I2C_FRAM
    model: MB85RC64
    address: 0x50
    mode: raw

on_boot:
  priority: -100
  then:
    - lambda: |-
        auto *storage = id(fram);
        uint32_t count = 0;
        storage->read(0, (uint8_t*)&count, 4);
        count++;
        storage->write(0, (uint8_t*)&count, 4);
        ESP_LOGI("boot", "Boot #%u", count);
```

### Data Logger (Flash + LittleFS)

```yaml
binary_storage:
  - id: flash
    type: SPI_FLASH
    model: W25Q32
    cs_pin: GPIO5
    mode: littlefs
    mount_path: /logs
    auto_format: true

sensor:
  - platform: dallas
    id: temp_sensor
    on_value:
      then:
        - lambda: |-
            FILE *f = fopen("/logs/temp.csv", "a");
            if (f) {
              fprintf(f, "%lu,%.1f\n", millis(), id(temp_sensor).state);
              fclose(f);
            }
```

### Multi-Device Configuration

```yaml
i2c:
  sda: GPIO21
  scl: GPIO22

spi:
  clk_pin: GPIO18
  mosi_pin: GPIO23
  miso_pin: GPIO19

binary_storage:
  # FRAM for frequent state updates
  - id: state_fram
    type: I2C_FRAM
    model: MB85RC256V
    address: 0x50
    mode: raw

  # EEPROM for configuration
  - id: config_eeprom
    type: I2C_EEPROM
    model: AT24C256
    address: 0x51
    mode: littlefs
    mount_path: /config

  # Flash for data logging
  - id: data_flash
    type: SPI_FLASH
    model: W25Q32
    cs_pin: GPIO5
    mode: littlefs
    mount_path: /data
```

## Platform Support

- **ESP32**: Full support (all device types, all modes)
- **ESP8266**: Raw mode only (no LittleFS support)
- **RP2040**: Raw mode only (no LittleFS support)

LittleFS mounting requires ESP-IDF framework.

## Notes

- LittleFS component is automatically loaded for ESP-IDF builds
- Device nodes are auto-generated as `/dev/{type}{index}` (e.g., `/dev/fram0`)
- Mount points default to `/{device_id}` if not specified
- Auto-format initializes LittleFS on first mount or after corruption
- FRAM and MRAM support unlimited writes - ideal for counters and frequent state updates
- Flash requires erase-before-write and has sector-level wear leveling

## See Also

- [storage](../storage/) - Unified storage management
- [I2C Bus](https://esphome.io/components/i2c.html)
- [SPI Bus](https://esphome.io/components/spi.html)
