#include "bq25186.h"

#include "esphome/core/log.h"

namespace esphome::bq25186 {

ESPHOME_LOG_TAG(TAG, "bq25186");

// Bits the configuration owns in each register; the rest are runtime state (VBAT_CTRL pg_mode,
// SYS_REG GPO level), reset triggers (SHIP_RST) or the device id (MASK_ID) and are kept as read
static constexpr std::array<uint8_t, BQ25186_CONFIG_REGISTER_COUNT> BQ25186_CONFIG_MASKS = {
    0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x1F, 0xEF, 0xFF, 0xF0,
};

bool BQ25186Component::write_register_(uint8_t reg, uint8_t value) {
  if (!this->write_byte(reg, value)) {
    ESP_LOGW(TAG, "Failed to write register 0x%02X", reg);
    return false;
  }
  return true;
}

bool BQ25186Component::update_register_(uint8_t reg, uint8_t mask, uint8_t value) {
  uint8_t reg_value;
  if (!this->read_byte(reg, &reg_value)) {
    ESP_LOGW(TAG, "Failed to read register 0x%02X", reg);
    return false;
  }
  reg_value = (reg_value & ~mask) | (value & mask);
  return this->write_register_(reg, reg_value);
}

bool BQ25186Component::apply_config(const BQ25186ConfigRegisters &regs) {
  // Setup rejects an unknown chip; the action must not program it either
  if (this->is_failed())
    return false;
  for (size_t i = 0; i < regs.size(); i++) {
    const uint8_t reg = BQ25186_REG_VBAT_CTRL + i;
    const bool ok = BQ25186_CONFIG_MASKS[i] == 0xFF ? this->write_register_(reg, regs[i])
                                                    : this->update_register_(reg, BQ25186_CONFIG_MASKS[i], regs[i]);
    if (!ok) {
      this->status_set_warning();
      return false;
    }
  }
  this->status_clear_warning();
  return true;
}

void BQ25186Component::setup() {
  uint8_t mask_id;
  if (!this->read_byte(BQ25186_REG_MASK_ID, &mask_id)) {
    ESP_LOGE(TAG, "Failed to communicate with BQ25186");
    this->mark_failed();
    return;
  }

  // Another charger at this address could lay its registers out differently, so do not write into it
  const uint8_t device_id = mask_id & 0x0F;
  if (device_id != 0x01) {
    ESP_LOGE(TAG, "Unexpected device id 0x%X (expected 0x1)", device_id);
    this->mark_failed();
    return;
  }

  if (this->configure_on_boot_ && !this->apply_config(this->setup_regs_)) {
    ESP_LOGE(TAG, "Failed to apply configuration");
    this->mark_failed();
  }
}

void BQ25186Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BQ25186:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with BQ25186 failed");
  }
}

}  // namespace esphome::bq25186
