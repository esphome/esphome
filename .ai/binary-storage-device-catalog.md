# Binary Storage Device Catalog

## Purpose
Document all similar I2C/SPI binary storage devices to understand quirks BEFORE designing the abstraction interface.

---

## I2C FRAM (Ferroelectric RAM)

### Manufacturers & Series
- **Fujitsu**: MB85RC series
- **Cypress/Infineon**: FM24 series, CY15B (also has SPI variants)

### Common Characteristics
- **Base I2C Address**: 0x50 (0b1010000)
- **Write Endurance**: 10^14 cycles (100 trillion!)
- **No Write Delays**: Instant write, no page buffer needed
- **Data Retention**: 10-38 years
- **Speed**: Full I2C speed (up to 1MHz on some)

### Addressing Modes (by capacity)

| Capacity | Address Bits | Addressing Method | Example Chips |
|----------|--------------|-------------------|---------------|
| 4 Kbit (512B) | 9-bit | 1 bit in I2C addr, 8 bits data | FM24C04, MB85RC04 |
| 16 Kbit (2KB) | 11-bit | 3 bits in I2C addr, 8 bits data | FM24CL16, MB85RC16 |
| 64 Kbit+ | 16-bit | Standard 2-byte address | MB85RC64, FM24CL64 |
| 1 Mbit+ | 32-bit | A16 in I2C addr, 2-byte address | MB85RC1M |

### Manufacturer IDs (via special read)
- **Fujitsu**: 0x00A
- **Cypress**: 0x004

### Key Quirks
- ✅ **No page writes** - can write any size instantly
- ✅ **No erase needed** - write directly
- ⚠️ **Density detection**: Can read manufacturer/product ID from special address 0xF8
- ⚠️ **Addressing varies by density** - cannot swap chip sizes without code change

### Current ESPHome Support
- ✅ Already implemented: `components/fram/` (supports FRAM, FRAM9, FRAM11, FRAM32)

---

## I2C EEPROM

### Manufacturers & Series
- **Microchip**: AT24C, 24AA (low voltage), 24LC (standard), 24FC (high speed)
- **STMicroelectronics**: M24C series
- **ON Semi**: CAT24C series
- **Rohm**: BR24G series
- **Fremont**: FT24C series

### Common Characteristics
- **Base I2C Address**: 0x50 (0b1010000) - configurable with A0/A1/A2 pins
- **Write Endurance**: 10^6 cycles (1 million)
- **Write Time**: 5ms typical (page write)
- **Data Retention**: 40-200 years
- **Speed**: 100-400 kHz (1MHz on FC variants)

### Addressing Modes & Page Sizes

| Model | Capacity | Address Bits | Page Size | Write Time |
|-------|----------|--------------|-----------|------------|
| 24C01 | 1 Kbit (128B) | 8-bit | 8 bytes | 5ms |
| 24C02 | 2 Kbit (256B) | 8-bit | 8 bytes | 5ms |
| 24C04 | 4 Kbit (512B) | 9-bit | 16 bytes | 5ms |
| 24C08 | 8 Kbit (1KB) | 10-bit | 16 bytes | 5ms |
| 24C16 | 16 Kbit (2KB) | 11-bit | 16 bytes | 5ms |
| 24C32 | 32 Kbit (4KB) | 12-bit (2-byte) | 32 bytes | 5ms |
| 24C64 | 64 Kbit (8KB) | 13-bit (2-byte) | 32 bytes | 5ms |
| 24C128 | 128 Kbit (16KB) | 14-bit (2-byte) | 64 bytes | 5ms |
| 24C256 | 256 Kbit (32KB) | 15-bit (2-byte) | 64 bytes | 5ms |
| 24C512 | 512 Kbit (64KB) | 16-bit (2-byte) | 128 bytes | 5ms |

### Device Address Configuration
- **Address Pins**: A0, A1, A2 allow 8 devices on same bus
- Example: A0=0, A1=0, A2=0 → 0x50; A0=1, A1=0, A2=0 → 0x51

### Key Quirks
- ⚠️ **Page Writes Only**: Must write within page boundaries (no rollover on most)
- ⚠️ **24C16 Special Case**: Read/write doesn't roll over page boundaries (others do)
- ⚠️ **Write Delay Required**: 5ms wait after write before next operation
- ⚠️ **Busy Polling**: Can poll with ACK to detect write completion
- ✅ **Block Addressing**: Smaller chips (24C04-24C16) use I2C address bits for memory blocks

### Current ESPHome Support
- ❌ **Not yet implemented** - but feature requested (Issue #2032)

---

## SPI FRAM

### Manufacturers & Series
- **Cypress/Infineon**: FM25 series, CY15B series

### Common Characteristics
- **Interface**: SPI (Mode 0 or 3)
- **Write Endurance**: 10^14 cycles
- **No Write Delays**: Instant write
- **Speed**: Up to 40 MHz SPI clock
- **Voltage**: 2-3.6V typical

### Models & Capacities

| Series | Capacity Range | Special Features |
|--------|----------------|------------------|
| FM25V | 1K - 256K | Standard voltage |
| FM25W | 256K - 1M | Wide temperature |
| FM25L | 16K - 256K | Low power |
| CY15B | 2M - 8M | High density, automotive grade |

### Addressing
- Standard SPI commands (WRITE, READ, WREN, WRDI, RDSR, WRSR)
- 16-bit or 24-bit address depending on capacity
- No special addressing quirks like I2C variants

### Key Quirks
- ✅ **Simple Protocol**: Standard SPI EEPROM command set
- ✅ **No Page Boundaries**: Can write any size
- ✅ **Status Register**: Contains write-enable latch, block protection bits

### Current ESPHome Support
- ❌ **Not yet implemented**

---

## SPI Flash (NOR Flash)

### Manufacturers & Series
- **Winbond**: W25Q series, W25X series
- **Macronix**: MX25L series, MX25R series, MX25U series
- **Adesto**: AT25DF series, AT25SF series
- **GigaDevice**: GD25Q series
- **Micron**: N25Q series

### Common Characteristics
- **Interface**: SPI (Mode 0 or 3), some support Quad-SPI
- **Write Endurance**: 100,000 cycles typical
- **Erase Required**: Must erase before write
- **Speed**: 50-133 MHz SPI clock (higher for Quad-SPI)

### Common Memory Organization

| Parameter | W25Q | MX25 | AT25 |
|-----------|------|------|------|
| **Page Size** | 256 bytes | 256 bytes | 256 bytes |
| **Sector Size** | 4 KB | 4 KB | 4 KB |
| **Block Size (32K)** | 32 KB | 32 KB | 32 KB |
| **Block Size (64K)** | 64 KB | 64 KB | 64 KB |
| **Sector Erase Time** | 50ms typ | 40ms typ | 100ms typ |
| **Block Erase Time** | 150ms typ | 400ms typ | 200ms typ |
| **Page Program Time** | 0.7ms typ | 1.4ms typ | 3ms typ |

### Standard Commands (JEDEC compatible)
- **0x03**: Read Data
- **0x02**: Page Program
- **0x20**: Sector Erase (4KB)
- **0x52**: Block Erase (32KB)
- **0xD8**: Block Erase (64KB)
- **0x06**: Write Enable
- **0x04**: Write Disable
- **0x05**: Read Status Register
- **0x9F**: Read JEDEC ID

### Key Quirks
- ⚠️ **Erase Before Write**: Must erase sector/block before programming
- ⚠️ **Page Boundary**: Page program must not cross 256-byte boundary
- ⚠️ **Write-Enable Required**: Must send WREN (0x06) before any write/erase
- ⚠️ **Busy Polling**: Check status register WIP bit after operations
- ✅ **JEDEC ID**: Can identify manufacturer/device with 0x9F command
- ⚠️ **AT25 Special**: Some Adesto chips support 256-byte erase (not just 4KB)

### Current ESPHome Support
- ❌ **Not yet implemented**

---

## Comparison Matrix

| Feature | I2C FRAM | I2C EEPROM | SPI FRAM | SPI Flash |
|---------|----------|------------|----------|-----------|
| **Write Cycles** | 10^14 | 10^6 | 10^14 | 10^5 |
| **Write Speed** | Instant | 5ms/page | Instant | 0.7-3ms/page |
| **Erase Needed** | No | No | No | Yes (50-400ms) |
| **Max Capacity** | 2 Mbit | 512 Kbit | 8 Mbit | 256 Mbit+ |
| **Interface Speed** | 400kHz-1MHz | 400kHz-1MHz | 40MHz | 50-133MHz |
| **Page Writes** | No limit | Required | No limit | Required (256B) |
| **Cost** | $$$$ | $ | $$$ | $$ |
| **Best For** | Frequent writes, logging | Config storage | High-speed logging | Firmware, large data |

---

## Implementation Implications

### Common Interface Requirements
All devices need:
1. **Basic I/O**: `read(addr, *buf, len)`, `write(addr, *buf, len)`
2. **Device Info**: `get_capacity()`, `get_page_size()`, `get_name()`
3. **Status**: `is_ready()`, `is_busy()`

### Device-Specific Requirements

**I2C devices (FRAM/EEPROM):**
- Address mode handling (9/11/16/32 bit)
- I2C address calculation with page bits

**EEPROM-specific:**
- Page write with boundary checking
- Write delay/polling

**Flash-specific:**
- Erase operations (sector/block)
- Write-enable sequence
- More complex status management

### LittleFS Requirements
LittleFS needs a block device interface with:
- `read(block, offset, *buf, size)`
- `prog(block, offset, *buf, size)` - program (write)
- `erase(block)` - erase block
- `sync()` - ensure writes are committed

**Translation needed:**
- FRAM/SPI-FRAM: Erase is no-op, direct write
- EEPROM: Erase is no-op, page-aligned writes
- Flash: Must implement proper erase/program cycle

---

## Recommendations for Phase 1

Start with **I2C FRAM + I2C EEPROM** because:
1. ✅ Very similar addressing patterns (already solved in existing FRAM)
2. ✅ Same bus (I2C)
3. ✅ EEPROM just adds page write + delay (simple extensions)
4. ✅ Both can use same LittleFS block device adapter (erase = no-op)
5. ✅ Tests shared interface before adding SPI complexity

**Add later:**
- SPI FRAM (easy once I2C done)
- SPI Flash (needs erase logic)
