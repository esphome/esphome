#include "bq25186.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::bq25186 {

ESPHOME_LOG_TAG(TAG, "bq25186");

// Bits the configuration owns in each register; the rest are runtime state (VBAT_CTRL pg_mode,
// SYS_REG GPO level), reset triggers (SHIP_RST) or the device id (MASK_ID) and are kept as read
static const uint8_t BQ25186_CONFIG_MASKS[BQ25186_CONFIG_REGISTER_COUNT] PROGMEM = {
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
  // Setup rejects an unknown chip; the action must not program it before that check or after it failed
  if (!this->is_ready() || this->is_failed()) {
    ESP_LOGW(TAG, "Not applying configuration, setup has not succeeded");
    return false;
  }
  for (size_t i = 0; i < regs.size(); i++) {
    const uint8_t reg = BQ25186_REG_VBAT_CTRL + i;
    const uint8_t mask = progmem_read_byte(&BQ25186_CONFIG_MASKS[i]);
    const bool ok = mask == 0xFF ? this->write_register_(reg, regs[i]) : this->update_register_(reg, mask, regs[i]);
    if (!ok) {
      this->status_set_warning(LOG_STR("Configuration partly applied"));
      return false;
    }
  }
  this->status_clear_warning();
  return true;
}

void BQ25186Component::setup() {
  uint8_t mask_id;
  if (!this->read_byte(BQ25186_REG_MASK_ID, &mask_id)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  // Another charger at this address could lay its registers out differently, so do not write into it
  const uint8_t device_id = mask_id & 0x0F;
  if (device_id != 0x01) {
    ESP_LOGE(TAG, "Unexpected device id 0x%X (expected 0x1)", device_id);
    this->mark_failed(LOG_STR("Not a BQ25186"));
    return;
  }

  // A failed write only sets the warning status: the chip has identified itself, so a later
  // apply_configuration may retry
  if (this->configure_on_boot_)
    this->apply_config(this->setup_regs_);
#ifdef USE_SWITCH
  if (this->pg_gpo_switch_ != nullptr) {
    // Configuration never touches these two bits, so the pin mode and level survive apply_config
    uint8_t sys_reg;
    if (!this->update_register_(BQ25186_REG_VBAT_CTRL, 0x80, 0x80) || !this->read_byte(BQ25186_REG_SYS_REG, &sys_reg)) {
      this->status_set_warning(LOG_STR("PG/GPO pin not in output mode"));
      return;
    }
    this->pg_gpo_switch_->publish_state((sys_reg & 0x10) != 0);
  }
#endif
  // The poller first runs after a random share of the interval; the entities get a state now
  this->update();
}

// Every poll also serves the I2C watchdog, which wants to see traffic within its period
void BQ25186Component::update() {
  BQ25186StatusRegisters status;
  if (!this->read_bytes(BQ25186_REG_STAT0, status.data(), status.size())) {
    this->status_set_warning(LOG_STR("Status read failed"));
    return;
  }
  this->status_clear_warning();
#ifdef BQ25186_LISTENER_COUNT
  for (auto *listener : this->listeners_) {
    listener->on_status(status);
  }
#endif
}

void BQ25186Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BQ25186:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
#ifdef USE_SWITCH
  LOG_SWITCH("  ", "PG/GPO", this->pg_gpo_switch_);
#endif
}

}  // namespace esphome::bq25186
