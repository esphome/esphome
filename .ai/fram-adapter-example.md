# FRAM Adapter Example

## How existing FRAM adapts to BinaryStorage interface

This shows how to retrofit the existing `components/fram/` to use the new `BinaryStorage` base.

---

## New Header: `components/fram/fram_binary_storage.h`

```cpp
#pragma once

#include "esphome/components/binary_storage/binary_storage.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace fram {

/**
 * @brief FRAM storage device using BinaryStorage interface
 *
 * Adapts existing FRAM implementation to the unified binary storage interface.
 * Supports 9/11/16/32-bit addressing modes.
 */
class FramBinaryStorage : public binary_storage::BinaryStorage, public i2c::I2CDevice {
 public:
  FramBinaryStorage() = default;

  // Component lifecycle
  void setup() override;
  void dump_config() override;

  // Device configuration
  void set_size_bytes(uint32_t size) { this->size_bytes_ = size; }
  void set_addressing_mode(AddressingMode mode) { this->addressing_mode_ = mode; }

  // BinaryStorage interface implementation
  uint32_t get_capacity() const override { return this->size_bytes_; }
  const char *get_device_name() const override { return "FRAM"; }
  const char *get_device_type() const override { return "fram"; }
  uint32_t get_page_size() const override { return 1; }  // FRAM has no page limit

  bool read(uint32_t address, uint8_t *data, size_t length) override;
  bool write(uint32_t address, const uint8_t *data, size_t length) override;

  // FRAM doesn't need erase or sync
  bool erase_block(uint32_t address) override { return true; }
  bool sync() override { return true; }

  // FRAM-specific metadata
  uint16_t get_manufacturer_id();
  uint16_t get_product_id();

 protected:
  enum class AddressingMode {
    ADDR_9BIT,   // 512 bytes
    ADDR_11BIT,  // 2 KB
    ADDR_16BIT,  // up to 64 KB
    ADDR_32BIT   // > 64 KB
  };

  uint32_t size_bytes_{0};
  AddressingMode addressing_mode_{AddressingMode::ADDR_16BIT};

  // Internal I/O operations (existing FRAM logic)
  void write_block_(uint16_t memaddr, uint8_t *obj, uint8_t size);
  void read_block_(uint16_t memaddr, uint8_t *obj, uint8_t size);
  uint16_t get_meta_data_(uint8_t field);
};

// Subclasses for different addressing modes (backward compatibility)
class Fram9BitStorage : public FramBinaryStorage {
 public:
  Fram9BitStorage() { this->set_addressing_mode(AddressingMode::ADDR_9BIT); }
};

class Fram11BitStorage : public FramBinaryStorage {
 public:
  Fram11BitStorage() { this->set_addressing_mode(AddressingMode::ADDR_11BIT); }
};

class Fram32BitStorage : public FramBinaryStorage {
 public:
  Fram32BitStorage() { this->set_addressing_mode(AddressingMode::ADDR_32BIT); }
};

}  // namespace fram
}  // namespace esphome
```

---

## Implementation: `components/fram/fram_binary_storage.cpp`

```cpp
#include "fram_binary_storage.h"
#include "esphome/core/log.h"

namespace esphome {
namespace fram {

static const char *const TAG = "fram";

void FramBinaryStorage::setup() {
  ESP_LOGCONFIG(TAG, "Setting up FRAM Binary Storage...");

  // Try to auto-detect size if not configured
  if (this->size_bytes_ == 0) {
    uint16_t density = this->get_meta_data_(2);  // Get density field
    if (density > 0) {
      this->size_bytes_ = (1UL << density) * 1024UL;  // Convert to bytes
      ESP_LOGI(TAG, "Auto-detected size: %u KB", this->size_bytes_ / 1024);
    } else {
      ESP_LOGW(TAG, "Could not auto-detect size, set it in config!");
    }
  }

  // Call base class setup
  binary_storage::BinaryStorage::setup();
}

void FramBinaryStorage::dump_config() {
  ESP_LOGCONFIG(TAG, "FRAM Binary Storage:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Size: %u bytes (%.1f KB)", this->size_bytes_, this->size_bytes_ / 1024.0f);
  ESP_LOGCONFIG(TAG, "  Manufacturer ID: 0x%04X", this->get_manufacturer_id());
  ESP_LOGCONFIG(TAG, "  Product ID: 0x%04X", this->get_product_id());
}

bool FramBinaryStorage::read(uint32_t address, uint8_t *data, size_t length) {
  if (!this->is_valid_address_(address, length)) {
    ESP_LOGE(TAG, "Read address out of bounds: 0x%X + %u", address, length);
    return false;
  }

  const uint8_t blocksize = 24;
  uint8_t *p = data;

  while (length >= blocksize) {
    this->read_block_(address, p, blocksize);
    address += blocksize;
    p += blocksize;
    length -= blocksize;
  }

  if (length > 0) {
    this->read_block_(address, p, length);
  }

  return true;
}

bool FramBinaryStorage::write(uint32_t address, const uint8_t *data, size_t length) {
  if (!this->is_valid_address_(address, length)) {
    ESP_LOGE(TAG, "Write address out of bounds: 0x%X + %u", address, length);
    return false;
  }

  const uint8_t blocksize = 24;
  const uint8_t *p = data;

  while (length >= blocksize) {
    this->write_block_(address, const_cast<uint8_t *>(p), blocksize);
    address += blocksize;
    p += blocksize;
    length -= blocksize;
  }

  if (length > 0) {
    this->write_block_(address, const_cast<uint8_t *>(p), length);
  }

  return true;
}

// Existing FRAM internal methods remain the same
void FramBinaryStorage::write_block_(uint16_t memaddr, uint8_t *obj, uint8_t size) {
  // Existing implementation from FRAM.cpp
  // ... (copy from existing code)
}

void FramBinaryStorage::read_block_(uint16_t memaddr, uint8_t *obj, uint8_t size) {
  // Existing implementation from FRAM.cpp
  // ... (copy from existing code)
}

uint16_t FramBinaryStorage::get_manufacturer_id() {
  return this->get_meta_data_(0);
}

uint16_t FramBinaryStorage::get_product_id() {
  return this->get_meta_data_(1);
}

uint16_t FramBinaryStorage::get_meta_data_(uint8_t field) {
  // Existing implementation from FRAM.cpp
  // ... (copy from existing code)
  return 0;
}

}  // namespace fram
}  // namespace esphome
```

---

## Python Config: `components/fram/__init__.py` (NEW VERSION)

```python
from esphome.components import binary_storage, i2c
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TYPE, CONF_SIZE

DEPENDENCIES = ["binary_storage", "i2c"]
MULTI_CONF = True

fram_ns = cg.esphome_ns.namespace("fram")
FramBinaryStorage = fram_ns.class_("FramBinaryStorage", binary_storage.BinaryStorage, i2c.I2CDevice)
Fram9BitStorage = fram_ns.class_("Fram9BitStorage", FramBinaryStorage)
Fram11BitStorage = fram_ns.class_("Fram11BitStorage", FramBinaryStorage)
Fram32BitStorage = fram_ns.class_("Fram32BitStorage", FramBinaryStorage)

def validate_bytes_1024(value):
    # Existing size validation logic
    # ...
    pass

FRAM_SCHEMA = cv.Schema({
    cv.Optional(CONF_SIZE): validate_bytes_1024
}).extend(cv.COMPONENT_SCHEMA).extend(i2c.i2c_device_schema(0x50))

CONFIG_SCHEMA = cv.typed_schema({
    "FRAM": FRAM_SCHEMA.extend({
        cv.GenerateID(): cv.declare_id(FramBinaryStorage)
    }),
    "FRAM9": FRAM_SCHEMA.extend({
        cv.GenerateID(): cv.declare_id(Fram9BitStorage)
    }),
    "FRAM11": FRAM_SCHEMA.extend({
        cv.GenerateID(): cv.declare_id(Fram11BitStorage)
    }),
    "FRAM32": FRAM_SCHEMA.extend({
        cv.GenerateID(): cv.declare_id(Fram32BitStorage)
    })
}, key=CONF_TYPE, default_type="FRAM", upper=True)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_SIZE in config:
        cg.add(var.set_size_bytes(config[CONF_SIZE]))

    # Register with storage_host via CORE.data
    from esphome.core import CORE
    if not hasattr(CORE, "data"):
        CORE.data = {}
    if "binary_storage_devices" not in CORE.data:
        CORE.data["binary_storage_devices"] = []
    CORE.data["binary_storage_devices"].append(var)
```

---

## Migration Benefits

1. **✅ Backward Compatible**: Existing FRAM YAML configs still work
2. **✅ Minimal Code Changes**: Mostly wrapping existing logic
3. **✅ New Capabilities**: Automatically gets LittleFS support
4. **✅ Unified Interface**: Works with storage_host
5. **✅ Clean Separation**: BinaryStorage interface, FRAM implementation

---

## Next: EEPROM Implementation

Similar pattern, but adds:
- Page write logic
- Write delay/polling
- Boundary checking

Would look like:
```cpp
class EepromBinaryStorage : public binary_storage::BinaryStorage, public i2c::I2CDevice {
  // ...
  uint32_t get_page_size() const override { return this->page_size_; }
  bool write(uint32_t address, const uint8_t *data, size_t length) override {
    // Handle page boundaries
    // Add write delays
  }
}
```
