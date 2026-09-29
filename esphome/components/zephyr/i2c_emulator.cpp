#include "i2c_emulator.h"

#ifdef USE_ZEPHYR

#include <cstring>

#include "esphome/core/log.h"

namespace esphome::zephyr {

static const char *const TAG = "zephyr.i2c_emulator";

void ZephyrI2CEmulator::add_register(uint8_t reg, const uint8_t *data, uint8_t entry_len, uint8_t seq_len) {
  if (this->registers_.size() >= this->registers_.capacity()) {
    ESP_LOGE(TAG, "Cannot register 0x%02X: emulator register table full", reg);
    return;
  }
  this->registers_.push_back(RegEntry{reg, data, entry_len, seq_len, 0});
}

ZephyrI2CEmulator::RegEntry *ZephyrI2CEmulator::find_entry_(uint8_t reg) {
  for (auto &entry : this->registers_) {
    if (entry.reg == reg)
      return &entry;
  }
  return nullptr;
}

int ZephyrI2CEmulator::transfer_s(const struct emul *target, struct i2c_msg *msgs, int num_msgs, int addr) {
  auto *self = static_cast<ZephyrI2CEmulator *>(target->data);
  return self->transfer_(msgs, num_msgs, addr);
}

int ZephyrI2CEmulator::transfer_(struct i2c_msg *msgs, int num_msgs, int addr) {
  if (addr != this->address_)
    return -EIO;
  for (int i = 0; i < num_msgs; i++) {
    struct i2c_msg &msg = msgs[i];
    if ((msg.flags & I2C_MSG_READ) == 0) {
      if (msg.len >= 1)
        this->current_reg_ = msg.buf[0];
      continue;
    }

    RegEntry *entry = this->find_entry_(this->current_reg_);
    if (entry == nullptr) {
      ESP_LOGW(TAG, "No emulated data for register 0x%02X on 0x%02X, returning zeros", this->current_reg_, addr);
      memset(msg.buf, 0, msg.len);
      continue;
    }

    const uint8_t *src = entry->data + (static_cast<size_t>(entry->index) * entry->entry_len);
    uint32_t copy_len = std::min<uint32_t>(msg.len, entry->entry_len);
    memcpy(msg.buf, src, copy_len);
    if (msg.len > copy_len)
      memset(msg.buf + copy_len, 0, msg.len - copy_len);
    entry->index = (entry->index + 1) % entry->seq_len;
  }
  return 0;
}

void ZephyrI2CEmulator::setup() {
  this->emul_api_.transfer = transfer_s;

  this->i2c_emul_.api = &this->emul_api_;
  this->i2c_emul_.addr = this->address_;
  this->i2c_emul_.target = &this->emul_;

  this->emul_.dev = this->i2c_dev_;
  this->emul_.data = this;
  this->emul_.bus.i2c = &this->i2c_emul_;

  i2c_emul_register(this->i2c_dev_, &this->i2c_emul_);
}

void ZephyrI2CEmulator::dump_config() {
  ESP_LOGCONFIG(TAG, "I2C Emulator 0x%02X: %u registers", this->address_,
                static_cast<unsigned>(this->registers_.size()));
}

}  // namespace esphome::zephyr

#endif
