# SPI Flash and SPI FRAM Implementation Summary

## What Was Added

This document summarizes the SPI Flash and SPI FRAM implementation added to the binary_storage component.

---

## New Files Created

### 1. SPI Flash Implementation
- **`spi_flash.h`** - Header file for SPI Flash devices
- **`spi_flash.cpp`** - Implementation for W25Q, MX25, AT25 series

**Key Features:**
- JEDEC ID reading for auto-detection
- Manufacturer identification (Winbond, Macronix, Adesto, etc.)
- Auto-configuration from JEDEC ID or model string
- Sector/block/chip erase operations (4KB, 32KB, 64KB)
- Page program with erase-before-write logic
- Wait-ready polling with timeout
- Power down/wake up support
- Address overflow protection

**Supported Commands:**
```cpp
CMD_READ_DATA        = 0x03  // Read data
CMD_PAGE_PROGRAM     = 0x02  // Program page
CMD_SECTOR_ERASE_4K  = 0x20  // Erase 4KB sector
CMD_BLOCK_ERASE_32K  = 0x52  // Erase 32KB block
CMD_BLOCK_ERASE_64K  = 0xD8  // Erase 64KB block
CMD_CHIP_ERASE       = 0xC7  // Erase entire chip
CMD_READ_STATUS_1    = 0x05  // Read status register
CMD_READ_JEDEC_ID    = 0x9F  // Read JEDEC ID
CMD_WRITE_ENABLE     = 0x06  // Enable write operations
CMD_POWER_DOWN       = 0xB9  // Enter power-down mode
CMD_RELEASE_POWER_DOWN = 0xAB // Exit power-down mode
```

### 2. SPI FRAM Implementation
- **`spi_fram.h`** - Header file for SPI FRAM devices
- **`spi_fram.cpp`** - Implementation for FM25, CY15B series

**Key Features:**
- Standard SPI EEPROM command set
- 16-bit or 24-bit addressing modes
- No erase needed (instant writes)
- No write delays required
- Status register operations
- Device ID reading
- Fast clear operation
- Address overflow protection

**Supported Commands:**
```cpp
CMD_WREN  = 0x06  // Set write enable latch
CMD_WRDI  = 0x04  // Reset write enable latch
CMD_RDSR  = 0x05  // Read status register
CMD_WRSR  = 0x01  // Write status register
CMD_READ  = 0x03  // Read memory data
CMD_WRITE = 0x02  // Write memory data
CMD_RDID  = 0x9F  // Read device ID (if supported)
```

### 3. Python Configuration Updates
- **`__init__.py`** - Updated to support SPI devices

**Changes Made:**
- Added `spi` component import
- Removed hard `i2c` dependency (now optional based on device type)
- Added `SPIFlash` and `SPIFram` class declarations
- Created `SPI_FLASH_SCHEMA` configuration schema
- Created `SPI_FRAM_SCHEMA` configuration schema
- Added SPI device types to `typed_schema`
- Updated `to_code()` to handle SPI device registration
- Added SPI-specific configuration options:
  - `erase_size`: Flash erase block size (4KB/32KB/64KB)
  - `jedec_id`: Override auto-detected JEDEC ID

### 4. Documentation Updates
- **`binary-storage-complete-summary.md`** - Updated with SPI device info
- **`binary-storage-example.yaml`** - Complete example with all 4 device types

---

## Configuration Examples

### SPI Flash

```yaml
spi:
  clk_pin: GPIO18
  mosi_pin: GPIO23
  miso_pin: GPIO19

binary_storage:
  - type: SPI_FLASH
    id: my_flash
    cs_pin: GPIO5
    model: W25Q32              # 4MB, auto-configured
    mode: littlefs
    mount_path: /data
    auto_format: true
```

**Manual Configuration:**
```yaml
binary_storage:
  - type: SPI_FLASH
    id: custom_flash
    cs_pin: GPIO5
    capacity: 4MB
    page_size: 256
    erase_size: 4096           # 4KB sector erase
    jedec_id: 0xEF4016         # Winbond W25Q32
    mode: raw
```

### SPI FRAM

```yaml
spi:
  clk_pin: GPIO18
  mosi_pin: GPIO23
  miso_pin: GPIO19

binary_storage:
  - type: SPI_FRAM
    id: my_fram
    cs_pin: GPIO15
    model: FM25V10             # 128KB, auto-configured
    mode: both                 # Binary + filesystem
    mount_path: /logs
    auto_format: true
```

**Manual Configuration:**
```yaml
binary_storage:
  - type: SPI_FRAM
    id: custom_fram
    cs_pin: GPIO15
    capacity: 128KB
    addressing_bits: 24        # 24-bit addressing
    mode: raw
```

---

## Supported Device Models

### SPI Flash

#### Winbond (Manufacturer ID: 0xEF)
| Model | JEDEC ID | Capacity | Notes |
|-------|----------|----------|-------|
| W25Q10 | 0xEF4011 | 128KB | 1Mbit |
| W25Q20 | 0xEF4012 | 256KB | 2Mbit |
| W25Q40 | 0xEF4013 | 512KB | 4Mbit |
| W25Q80 | 0xEF4014 | 1MB | 8Mbit |
| W25Q16 | 0xEF4015 | 2MB | 16Mbit |
| W25Q32 | 0xEF4016 | 4MB | 32Mbit |
| W25Q64 | 0xEF4017 | 8MB | 64Mbit |
| W25Q128 | 0xEF4018 | 16MB | 128Mbit |

#### Macronix (Manufacturer ID: 0xC2)
| Model | JEDEC ID | Capacity | Notes |
|-------|----------|----------|-------|
| MX25L512 | 0xC22010 | 64KB | 512Kbit |
| MX25L1006E | 0xC22011 | 128KB | 1Mbit |
| MX25L2006E | 0xC22012 | 256KB | 2Mbit |
| MX25L4006E | 0xC22013 | 512KB | 4Mbit |
| MX25L8006E | 0xC22014 | 1MB | 8Mbit |
| MX25L1606E | 0xC22015 | 2MB | 16Mbit |
| MX25L3206E | 0xC22016 | 4MB | 32Mbit |

#### Adesto (Manufacturer ID: 0x1F)
| Model | JEDEC ID | Capacity | Notes |
|-------|----------|----------|-------|
| AT25SF041 | 0x1F8401 | 512KB | 4Mbit |
| AT25SF081 | 0x1F8501 | 1MB | 8Mbit |
| AT25SF161 | 0x1F8601 | 2MB | 16Mbit |
| AT25SF321 | 0x1F8701 | 4MB | 32Mbit |

### SPI FRAM

#### Cypress/Infineon (FM25 Series)
| Model | Capacity | Addressing | Max SPI Speed |
|-------|----------|------------|---------------|
| FM25V01 | 16KB | 16-bit | 40MHz |
| FM25V02 | 32KB | 16-bit | 40MHz |
| FM25V05 | 64KB | 16-bit | 40MHz |
| FM25V10 | 128KB | 24-bit | 40MHz |
| FM25V20 | 256KB | 24-bit | 40MHz |
| FM25V40 | 512KB | 24-bit | 40MHz |

#### Cypress (CY15B Series)
| Model | Capacity | Addressing | Max SPI Speed |
|-------|----------|------------|---------------|
| CY15B102Q | 256KB | 24-bit | 40MHz |
| CY15B104Q | 512KB | 24-bit | 40MHz |

---

## Device Characteristics Comparison

| Feature | SPI Flash | SPI FRAM |
|---------|-----------|----------|
| **Write Speed** | ~3ms per page | Instant (<1μs) |
| **Erase Required** | Yes (45-500ms) | No |
| **Write Endurance** | 100K cycles/sector | 100 trillion cycles |
| **Capacity Range** | 128KB - 16MB | 16KB - 512KB |
| **Max SPI Speed** | 104MHz (Quad SPI) | 40MHz |
| **Power Consumption** | Higher (erase) | Lower |
| **Cost per MB** | Low | High |
| **Best For** | Large storage | Frequent writes |

---

## Implementation Details

### SPI Flash Write Algorithm

1. **Check alignment**: Ensure write doesn't cross page boundary
2. **Wait for ready**: Poll status register until WIP=0
3. **Write enable**: Send WREN command
4. **Program page**: Send PAGE_PROGRAM command + address + data
5. **Wait for completion**: Poll status register
6. **Repeat**: Continue for remaining data

### SPI FRAM Write Algorithm

1. **Write enable**: Send WREN command
2. **Write data**: Send WRITE command + address + data
3. **Write disable**: Send WRDI command (optional)
4. **Done**: No waiting required!

### JEDEC ID Format

```
Byte 0: Manufacturer ID (e.g., 0xEF = Winbond)
Byte 1: Memory Type (e.g., 0x40 = SPI Flash)
Byte 2: Capacity (e.g., 0x16 = 2^22 bytes = 4MB)
```

Auto-detection uses this to configure:
- Manufacturer name
- Capacity (2^byte2 bytes)
- Standard page size (256 bytes)
- Standard erase size (4KB sectors)

---

## Testing Recommendations

### SPI Flash Testing

1. **Basic Operations**
   - Test read/write to page boundaries
   - Verify erase operations (4KB/32KB/64KB)
   - Check JEDEC ID reading
   - Test manufacturer detection

2. **Performance**
   - Measure page program time (~3ms expected)
   - Measure sector erase time (~45ms expected)
   - Test continuous read speed

3. **LittleFS Integration**
   - Test filesystem mount
   - Create/read/write/delete files
   - Test persistence across reboots
   - Test power-fail safety (ESP-IDF LittleFS is power-safe)

4. **Edge Cases**
   - Full chip erase
   - Write to last address
   - Corrupt JEDEC ID
   - Power-down/wake-up

### SPI FRAM Testing

1. **Basic Operations**
   - Test instant writes (no delay)
   - Verify status register
   - Test device ID reading (if supported)
   - Test 16-bit vs 24-bit addressing

2. **Performance**
   - Measure write speed (should be <10μs)
   - Test maximum write size
   - Test continuous read/write

3. **Endurance**
   - Write same location 1M+ times
   - Verify no degradation

4. **LittleFS Integration**
   - Test filesystem operations
   - Verify persistence
   - Test with frequent writes (FRAM excels here)

---

## Known Limitations

1. **SPI Flash**
   - Must erase before write (adds latency)
   - Limited write endurance (100K cycles per sector)
   - Erase granularity is coarse (4KB minimum)

2. **SPI FRAM**
   - Higher cost per MB than Flash
   - Lower capacity than Flash
   - Device ID command may not be supported on all chips

3. **Both**
   - Requires SPI bus (3-4 GPIOs)
   - CS pin required per device
   - No hardware write protection implemented yet

---

## Future Enhancements

1. **Quad SPI (QSPI)**
   - 4x faster reads using 4 data lines
   - Requires QSPI-capable pins on ESP32

2. **Hardware Write Protection**
   - Use WP# pin for Flash write protection
   - Implement software block protection

3. **Wear Leveling**
   - Optional wear leveling layer for Flash
   - Extends lifetime for frequently-written locations

4. **OTA Support**
   - Store firmware images on Flash
   - Integrate with ESPHome OTA system

---

## Statistics

### Code Size
- **spi_flash.h**: ~250 lines
- **spi_flash.cpp**: ~900 lines
- **spi_fram.h**: ~170 lines
- **spi_fram.cpp**: ~290 lines
- **Total new C++ code**: ~1,610 lines

### Python Configuration
- **Added schemas**: SPI_FLASH_SCHEMA, SPI_FRAM_SCHEMA
- **New config options**: erase_size, jedec_id
- **Device type aliases**: FLASH → SPI_FLASH, SPI_FRAM

### Device Support
- **SPI Flash models**: 18+ (Winbond, Macronix, Adesto)
- **SPI FRAM models**: 10+ (Cypress/Infineon FM25, CY15B)

---

## Ready for Testing! 🚀

The SPI Flash and SPI FRAM implementations are complete and ready for real-world testing. All features match the existing I2C implementations, with device-specific optimizations for Flash erase operations and FRAM instant writes.

**Next steps:**
1. Test with actual hardware
2. Verify auto-detection works correctly
3. Test LittleFS integration
4. Benchmark performance vs expectations
5. Test all automation actions

**Recommended first test:**
Use SPI FRAM with mode=raw for simplest testing (no filesystem complexity, instant writes).
