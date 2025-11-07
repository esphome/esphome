# Binary Storage Usage Guide

## What We Built

A complete unified binary storage system for ESPHome supporting:
- ✅ **I2C EEPROM** (AT24C/24LC/24AA series) - 1Kbit to 512Kbit
- ✅ **I2C FRAM** (MB85RC, FM24 series) - Instant writes, 100 trillion cycles
- ✅ **LittleFS Filesystem** - Mount storage devices as filesystems

---

## Quick Start Examples

### Example 1: Basic EEPROM (No Filesystem)

```yaml
i2c:
  sda: GPIO21
  scl: GPIO22

binary_storage:
  - type: EEPROM
    id: my_eeprom
    model: AT24C256  # Auto-configures to 32KB, 64-byte pages
    address: 0x50
```

**What you get:**
- 32KB EEPROM storage
- Auto-detected page size (64 bytes)
- Auto-detected addressing mode (16-bit)
- Direct read/write API

### Example 2: FRAM with LittleFS Filesystem

```yaml
i2c:
  sda: GPIO21
  scl: GPIO22

binary_storage:
  - type: FRAM
    id: my_fram
    model: MB85RC256  # Auto-configures to 32KB
    address: 0x50
    mount_path: /fram  # Mount as filesystem!
    auto_format: true
```

**What you get:**
- 32KB FRAM storage with instant writes
- Mounted at `/fram` as LittleFS filesystem
- Auto-formats on first boot
- Standard file operations (fopen, fwrite, etc.)

### Example 3: Manual Configuration

```yaml
binary_storage:
  - type: EEPROM
    id: custom_eeprom
    model: Custom EEPROM
    capacity: 64KB
    page_size: 128
    addressing_bits: 16
    address: 0x50
```

**When to use:**
- Non-standard chips
- Override auto-detection
- Fine-tune parameters

### Example 4: Multiple Devices

```yaml
i2c:
  sda: GPIO21
  scl: GPIO22

binary_storage:
  # Small FRAM for config
  - type: FRAM
    id: config_fram
    model: MB85RC64
    address: 0x50
    mount_path: /config

  # Large EEPROM for data
  - type: EEPROM
    id: data_eeprom
    model: AT24C512
    address: 0x51
    mount_path: /data
```

---

## API Reference

### Direct Storage Access (No Filesystem)

```cpp
// Get storage component
auto *storage = id(my_eeprom);

// Read data
uint8_t buffer[100];
bool success = storage->read(0x0000, buffer, 100);

// Write data
uint8_t data[] = {0x01, 0x02, 0x03};
success = storage->write(0x0000, data, 3);

// Get device info
uint32_t capacity = storage->get_capacity();
const char *type = storage->get_device_type();
```

### Filesystem Access (With mount_path)

```cpp
// Standard C file operations
FILE *f = fopen("/fram/config.txt", "w");
fprintf(f, "Hello from FRAM!\n");
fclose(f);

// Read back
f = fopen("/fram/config.txt", "r");
char buffer[100];
fgets(buffer, sizeof(buffer), f);
fclose(f);
```

---

## Supported Devices

### I2C EEPROM (AT24C Series)

| Model | Capacity | Page Size | Addressing |
|-------|----------|-----------|------------|
| AT24C01 | 128 B | 8 B | 8-bit |
| AT24C02 | 256 B | 8 B | 8-bit |
| AT24C04 | 512 B | 16 B | 9-bit |
| AT24C08 | 1 KB | 16 B | 10-bit |
| AT24C16 | 2 KB | 16 B | 11-bit |
| AT24C32 | 4 KB | 32 B | 16-bit |
| AT24C64 | 8 KB | 32 B | 16-bit |
| AT24C128 | 16 KB | 64 B | 16-bit |
| AT24C256 | 32 KB | 64 B | 16-bit |
| AT24C512 | 64 KB | 128 B | 16-bit |

**Compatible with:** 24LC, 24AA, M24C, CAT24C, BR24G, FT24C series

### I2C FRAM (Fujitsu/Cypress)

| Model | Capacity | Addressing | Special Features |
|-------|----------|------------|------------------|
| MB85RC04 | 512 B | 9-bit | |
| MB85RC16 | 2 KB | 11-bit | |
| MB85RC64 | 8 KB | 16-bit | |
| MB85RC256 | 32 KB | 16-bit | |
| MB85RC512 | 64 KB | 16-bit | |
| MB85RC1M | 128 KB | 32-bit | |

**Unique FRAM Benefits:**
- ⚡ Instant writes (no 5ms delay like EEPROM)
- 🔄 100 trillion write cycles (vs 1 million for EEPROM)
- 📅 10-38 year data retention
- 💤 Sleep mode for power saving

---

## Configuration Options

### Device Configuration

```yaml
binary_storage:
  - type: EEPROM | FRAM | I2C_EEPROM | I2C_FRAM
    id: device_id

    # Device settings
    model: "AT24C256"          # Model name (auto-configures parameters)
    address: 0x50              # I2C address (default 0x50)

    # Override auto-detection (optional)
    capacity: 32KB             # Total capacity
    page_size: 64              # Page size (EEPROM only)
    addressing_bits: 16        # 8, 9, 10, 11, 16, or 32 (FRAM: 9, 11, 16, 32)

    # Filesystem mounting (optional)
    mount_path: /fram          # Mount point in VFS
    auto_format: true          # Format on mount failure (default: true)
    partition_label: storage   # LittleFS partition label (optional)
```

### Size Validation

Supports units:
- `capacity: 256` - bytes
- `capacity: 32KB` - kilobytes (decimal)
- `capacity: 32KiB` - kibibytes (binary)
- `capacity: 1MB` - megabytes

---

## Advanced Features

### Custom Block Configuration

The BinaryStorage interface provides optimal block device configuration:

```cpp
BlockDeviceConfig config = storage->get_block_config();

// For EEPROM/FRAM: 4KB blocks (LittleFS default)
// For Flash: Uses erase block size

config.block_size;    // Size of each block
config.block_count;   // Total number of blocks
config.read_size;     // Minimum read size (1 byte)
config.prog_size;     // Program size (page size for EEPROM, 1 for FRAM)
```

### LittleFS Operations

```cpp
// Get mount component
auto *mount = id(my_fram_mount);

// Check mount status
bool mounted = mount->is_mounted();

// Manual operations
mount->unmount();
mount->remount();
mount->format();  // WARNING: Erases all data!
```

### FRAM-Specific Features

```cpp
auto *fram = id(my_fram);

// Read device metadata
uint16_t mfg_id = fram->get_manufacturer_id();  // 0x00A = Fujitsu, 0x004 = Cypress
uint16_t prod_id = fram->get_product_id();
uint16_t density = fram->get_density();  // Size = 2^density KB

// Fast clear
fram->clear(0x00);  // Fill with 0x00

// Power saving
fram->sleep();
fram->wakeup(400);  // 400us recovery time
```

---

## Performance Comparison

| Operation | EEPROM | FRAM | Notes |
|-----------|--------|------|-------|
| Write byte | 5ms | <1μs | FRAM is 5000x faster |
| Write page | 5ms | <10μs | FRAM has no page limit |
| Read | ~400kHz | ~1MHz | I2C speed limited |
| Endurance | 1M cycles | 100T cycles | FRAM lasts 100 million times longer |
| Erase needed | No | No | Both are byte-writable |

**When to use EEPROM:**
- ✅ Cost-sensitive applications
- ✅ Infrequent writes (config storage)
- ✅ Large capacity needs (up to 64KB easily)

**When to use FRAM:**
- ✅ Frequent writes (logging, counters)
- ✅ Instant write requirement (critical data)
- ✅ High endurance needs
- ✅ Power-fail safety (no write delay)

---

## Integration with storage_host

The binary_storage devices automatically register with storage_host:

```yaml
storage_host:

binary_storage:
  - type: FRAM
    id: my_fram
    mount_path: /fram

# Now accessible via storage_host
# storage_host->file_exists("/fram/config.txt")
# storage_host->read_file("/fram/data.bin")
```

---

## Troubleshooting

### Device Not Found

```
[E][i2c_eeprom:XX] Device not found at address 0x50
```

**Solutions:**
- Check I2C wiring (SDA, SCL, GND, VCC)
- Verify I2C address (use i2c scan)
- Check pull-up resistors (4.7kΩ typical)
- Verify A0/A1/A2 pins on EEPROM

### Mount Failed

```
[E][littlefs_mount:XX] Failed to mount LittleFS: ESP_FAIL
```

**Solutions:**
- Set `auto_format: true` (formats on first mount)
- Check capacity is correct
- Verify device is working (test with direct read/write first)

### Write Timeout (EEPROM)

```
[W][i2c_eeprom:XX] Write timeout after 10 ms
```

**Solutions:**
- EEPROM needs 5ms per page write
- Reduce write frequency
- Consider using FRAM instead

---

## What's Next?

Future additions (not yet implemented):
- SPI FRAM (FM25 series)
- SPI Flash (W25Q, MX25, AT25 series)
- Network storage (WebDAV, NFS)
- Raw device mode for http_file_server

See `.ai/binary-storage-plan.md` for roadmap.
