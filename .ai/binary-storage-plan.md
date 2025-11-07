# Binary Storage Integration Plan

## Goal
Extend `storage_host` component to support I2C/SPI binary storage devices (FRAM, EEPROM, Flash) with optional filesystem mounting.

## Architecture Decision: Hybrid Approach ✅

```
binary_storage/              # New unified component
├── __init__.py             # Base schema & type selection
├── binary_storage.h        # Abstract base interface
├── i2c/
│   ├── i2c_binary_storage.h    # I2C-specific base
│   ├── fram.h                  # Migrate existing FRAM
│   └── eeprom.h                # AT24C series (NEW)
└── spi/
    ├── spi_binary_storage.h    # SPI-specific base
    ├── flash.h                 # W25Q series (NEW)
    └── fram.h                  # SPI FRAM (FUTURE)
```

## Key Insights

### a) Addressing Similarities - YES!
- I2C devices share similar addressing:
  - **9-bit** (512B): 1 bit in I2C addr, 8 bits in data
  - **11-bit** (2KB): 3 bits in I2C addr, 8 bits in data
  - **16-bit** (64KB): 2-byte address
  - **32-bit** (>64KB): Use A16 in I2C device addr
- Current FRAM already handles this with FRAM9/FRAM11/FRAM32 variants
- Same pattern works for AT24C EEPROMs!

### b) Component Organization
**✅ Shared base interface + device-specific implementations**
- Common API for storage_host
- Device quirks isolated (page size, write delays)
- Easy to add new devices

### c) Filesystem Support - LittleFS
- **ESP-IDF component**: `joltwallet/littlefs` v1.14.8
- Power-safe, wear-leveling
- Small RAM footprint (~600 bytes)
- Perfect for FRAM/EEPROM (works on any block device)

### d) http_file_server Integration - Two Modes

**Mode 1: Filesystem Mount** (for general use)
```yaml
binary_storage:
  - id: my_fram
    type: fram
    filesystem: littlefs

storage_host:
  mounts:
    - path: /fram
      platform: binary_storage
```

**Mode 2: Raw Device** (for flashing/debugging)
```yaml
binary_storage:
  - id: my_fram
    type: fram
    expose_as_device: true

storage_host:
  virtual_devices:
    - path: /dev/fram0
```

## Network Storage
- ❌ **SMB/CIFS** - Too heavy for ESP32
- ✅ **WebDAV** - HTTP-based, lighter
- ✅ **TFTP** - Very lightweight
- ✅ **NFS** - Lighter than SMB

## Implementation Priority

### Phase 1: FRAM Integration with storage_host
1. Create minimal `BinaryStorage` base interface
2. Migrate existing FRAM to use it
3. Add LittleFS VFS support
4. Test mounting via storage_host

### Phase 2: Add I2C EEPROM
1. Implement AT24C support using FRAM pattern
2. Support AT24C02/04/08/16 (9-11 bit addressing)
3. Support AT24C32/64/128/256 (16-bit addressing)

### Phase 3: http_file_server Integration
1. Filesystem mode (auto-works via storage_host)
2. Raw device mode with flash/erase endpoints

### Phase 4: SPI Devices (FUTURE)
- W25Q flash series
- SPI FRAM

## Devices to Implement Together

**High Priority - Similar API:**
1. **FRAM** (existing) - I2C, 9/11/16/32-bit addressing
2. **AT24C EEPROM** - I2C, same addressing patterns
3. **MB85RC (Fujitsu FRAM)** - Compatible with existing

**References Found:**
- `nopnop2002/esp-idf-24c` - ESP-IDF library for AT24C
- RobTillaart/FRAM_I2C - Base for current FRAM (already integrated)

## Next Steps
- Start with Phase 1: Get FRAM mountable in storage_host
- Add LittleFS support
- Then extend to AT24C EEPROM (very similar to FRAM)
