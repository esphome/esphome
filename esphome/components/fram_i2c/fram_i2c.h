#pragma once

#include "esphome/components/nvm/nvm.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace nvm {

/// FRAM model definitions
struct FramModel {
  uint32_t size_bytes;    ///< Size in bytes
  uint8_t address_width;  ///< Address width in bytes (2 for 16-bit, 3 for 17/18-bit)
  const char *name;       ///< Model name for logging
};

/// Supported I2C FRAM models
static const FramModel FRAM_I2C_MODELS[] = {
    {8 * 1024, 2, "MB85RC64"},    // 64 Kbit = 8 KB
    {16 * 1024, 2, "MB85RC128"},  // 128 Kbit = 16 KB
    {32 * 1024, 2, "MB85RC256"},  // 256 Kbit = 32 KB
    {64 * 1024, 3, "MB85RC512"},  // 512 Kbit = 64 KB (17-bit address)
    {128 * 1024, 3, "MB85RC1M"},  // 1 Mbit = 128 KB (18-bit address)
};

/// I2C FRAM platform for NVM component
///
/// This platform supports Fujitsu MB85RC series I2C FRAM devices.
/// FRAM features:
/// - Unlimited write cycles (no wear)
/// - No write delays (instant writes)
/// - Fast read/write operations
/// - Non-volatile storage
///
/// Supported models:
/// - MB85RC64: 8 KB, 16-bit addressing
/// - MB85RC128: 16 KB, 16-bit addressing
/// - MB85RC256: 32 KB, 16-bit addressing
/// - MB85RC512: 64 KB, 17-bit addressing (uses address bits in device address)
/// - MB85RC1M: 128 KB, 18-bit addressing (uses address bits in device address)
class FramI2cPlatform : public NvmPlatform, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  /// Run at IO priority (1500) so hardware is ready before partitions (BUS = 2000)
  float get_setup_priority() const override { return setup_priority::IO; }

  /// Set FRAM model by size
  void set_model(uint32_t size_bytes);

  /// Set FRAM model directly
  void set_model_config(uint32_t size_bytes, uint8_t address_width, const char *name) {
    model_.size_bytes = size_bytes;
    model_.address_width = address_width;
    model_.name = name;
  }

  // ========== NvmPlatform overrides ==========
  bool read_bytes(uint32_t memaddr, uint8_t *data, size_t len) override;
  bool write_bytes(uint32_t memaddr, const uint8_t *data, size_t len) override;
  uint32_t get_total_size() const override { return model_.size_bytes; }

  /// Check if FRAM is connected
  bool is_connected();

 protected:
  /// Read with 16-bit address (standard I2C FRAM)
  bool read_bytes_16(uint32_t memaddr, uint8_t *data, size_t len);

  /// Write with 16-bit address (standard I2C FRAM)
  bool write_bytes_16(uint32_t memaddr, const uint8_t *data, size_t len);

  /// Read with extended address (17/18-bit via I2C)
  bool read_bytes_ext(uint32_t memaddr, uint8_t *data, size_t len);

  /// Write with extended address (17/18-bit via I2C)
  bool write_bytes_ext(uint32_t memaddr, const uint8_t *data, size_t len);

  FramModel model_{32 * 1024, 2, "MB85RC256"};  // Default: MB85RC256
};

}  // namespace nvm
}  // namespace esphome
