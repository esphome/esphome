#include "bq25186.h"

#include "esphome/core/log.h"

namespace esphome::bq25186 {

ESPHOME_LOG_TAG(TAG, "bq25186");

// SHIP_RST bits 7:5 trigger resets and MASK_ID bits 3:0 hold the device id
static constexpr uint8_t BQ25186_SHIP_RST_CONFIG_MASK = 0x1F;
static constexpr uint8_t BQ25186_MASK_ID_CONFIG_MASK = 0xF0;

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
  for (uint8_t reg = BQ25186_REG_VBAT_CTRL; reg <= BQ25186_REG_MASK_ID; reg++) {
    const uint8_t value = regs[reg - BQ25186_REG_VBAT_CTRL];
    bool ok;
    if (reg == BQ25186_REG_SHIP_RST) {
      ok = this->update_register_(reg, BQ25186_SHIP_RST_CONFIG_MASK, value);
    } else if (reg == BQ25186_REG_MASK_ID) {
      ok = this->update_register_(reg, BQ25186_MASK_ID_CONFIG_MASK, value);
    } else {
      ok = this->write_register_(reg, value);
    }
    if (!ok)
      return false;
  }
  return true;
}

bool BQ25186Component::write_pg_gpo_level(bool level) {
  uint8_t vbat_ctrl;
  if (!this->read_byte(BQ25186_REG_VBAT_CTRL, &vbat_ctrl)) {
    ESP_LOGW(TAG, "Failed to read register 0x%02X", BQ25186_REG_VBAT_CTRL);
    return false;
  }
  if ((vbat_ctrl & 0x80) == 0) {
    ESP_LOGW(TAG, "Ignoring PG/GPO level write because pg_mode is power_good");
    return false;
  }
  return this->update_register_(BQ25186_REG_SYS_REG, 0x10, level ? 0x10 : 0x00);
}

bool BQ25186Component::trigger_software_reset() { return this->update_register_(BQ25186_REG_SHIP_RST, 0x80, 0x80); }

bool BQ25186Component::trigger_shutdown_mode() { return this->update_register_(BQ25186_REG_SHIP_RST, 0x60, 0x20); }

bool BQ25186Component::trigger_ship_mode() { return this->update_register_(BQ25186_REG_SHIP_RST, 0x60, 0x40); }

bool BQ25186Component::trigger_hardware_reset() { return this->update_register_(BQ25186_REG_SHIP_RST, 0x60, 0x60); }

void BQ25186Component::setup() {
  uint8_t mask_id;
  if (!this->read_byte(BQ25186_REG_MASK_ID, &mask_id)) {
    ESP_LOGE(TAG, "Failed to communicate with BQ25186");
    this->mark_failed();
    return;
  }

  const uint8_t device_id = mask_id & 0x0F;
  if (device_id != 0x01) {
    ESP_LOGW(TAG, "Unexpected device id 0x%X (expected 0x1)", device_id);
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
