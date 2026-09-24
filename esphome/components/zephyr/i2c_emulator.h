#pragma once

#ifdef USE_ZEPHYR

#include <cstdint>

#include "esphome/core/component.h"

extern "C" {
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2c_emul.h>
}

namespace esphome::zephyr {

/// Answers I2C transfers for an emulated target device from a static,
/// optionally-cycling register map configured via `emulation: registers:`.
///
/// On native_sim it's a fallback after a real-bus NACK; on physical targets
/// Zephyr's i2c_transfer() dispatch routes registered addresses here directly.
class ZephyrI2CEmulator : public Component {
 public:
  ZephyrI2CEmulator(const device *i2c_dev, uint16_t address, size_t reg_count) : i2c_dev_(i2c_dev), address_(address) {
    this->registers_.init(reg_count);
  }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS + 1.0f; }

  /// Register a (possibly cycling) response for reads of `reg`.
  ///
  /// `data` points to `seq_len` consecutive entries of `entry_len` bytes each.
  /// Each read of `reg` returns the entry at the current cycle index, then
  /// advances `index = (index + 1) % seq_len`.
  void add_register(uint8_t reg, const uint8_t *data, uint8_t entry_len, uint8_t seq_len);

 protected:
  struct RegEntry {
    uint8_t reg{0};
    const uint8_t *data{nullptr};
    uint8_t entry_len{0};
    uint8_t seq_len{0};
    uint8_t index{0};
  };

  static int transfer_s(const struct emul *target, struct i2c_msg *msgs, int num_msgs, int addr);
  int transfer_(struct i2c_msg *msgs, int num_msgs, int addr);
  RegEntry *find_entry_(uint8_t reg);

  const device *i2c_dev_;
  uint16_t address_;
  uint8_t current_reg_{0};
  FixedVector<RegEntry> registers_;

  i2c_emul_api emul_api_{};
  i2c_emul i2c_emul_{};
  emul emul_{};
};

}  // namespace esphome::zephyr

#endif
