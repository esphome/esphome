#include "bq25186.h"
#ifdef USE_BINARY_SENSOR
#include "bq25186_binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "bq25186_text_sensor.h"
#endif

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::bq25186 {

ESPHOME_LOG_TAG(TAG, "bq25186");

// Bits the configuration owns in each register; the rest are the SYS_REG GPO level, reset
// triggers (SHIP_RST) or the device id (MASK_ID) and are kept as read
static const uint8_t BQ25186_CONFIG_MASKS[BQ25186_CONFIG_REGISTER_COUNT] PROGMEM = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x1F, 0xEF, 0xFF, 0xF0,
};

bool BQ25186Component::write_register_(uint8_t reg, uint8_t value) {
  if (!this->write_byte(reg, value)) {
    ESP_LOGW(TAG, "Failed to write register 0x%02X", reg);
    return false;
  }
  return true;
}

bool BQ25186Component::update_register_(uint8_t reg, uint8_t mask, uint8_t value) {
  if (this->update_register(reg, mask, value))
    return true;
  ESP_LOGW(TAG, "Failed to update register 0x%02X", reg);
  return false;
}

uint8_t BQ25186Component::expected_register_(size_t index) const {
  uint8_t value = this->regs_[index];
#ifdef USE_BQ25186_PG_GPO
  if (index == 0 && this->pg_gpo_switch_ != nullptr)
    value |= BQ25186_VBAT_CTRL_PG_MODE;
#endif
  return value;
}

bool BQ25186Component::config_matches_(const BQ25186StatusRegisters &regs) const {
  for (uint8_t reg = BQ25186_REG_VBAT_CTRL; reg <= BQ25186_REG_MASK_ID; reg++) {
    const size_t index = reg - BQ25186_REG_VBAT_CTRL;
    const uint8_t mask = progmem_read_byte(&BQ25186_CONFIG_MASKS[index]);
    if (((regs[reg] ^ this->expected_register_(index)) & mask) != 0)
      return false;
  }
  return true;
}

bool BQ25186Component::apply_config(const BQ25186ConfigRegisters &regs) {
  // Setup rejects an unknown chip; the action must not program it before that check or after it failed
  if (!this->is_ready() || this->is_failed()) {
    ESP_LOGW(TAG, "Not applying configuration, setup has not succeeded");
    return false;
  }
  this->regs_ = regs;
  this->configured_ = true;
  for (size_t i = 0; i < regs.size(); i++) {
    const uint8_t reg = BQ25186_REG_VBAT_CTRL + i;
    const uint8_t mask = progmem_read_byte(&BQ25186_CONFIG_MASKS[i]);
    const uint8_t value = this->expected_register_(i);
    const bool ok = mask == 0xFF ? this->write_register_(reg, value) : this->update_register_(reg, mask, value);
    if (!ok) {
      this->status_set_warning(LOG_STR("Configuration partly applied"));
      return false;
    }
  }
  this->status_clear_warning();
  return true;
}

void BQ25186Component::setup() {
  // Another charger at this address could lay its registers out differently, so do not write into it
  if (!this->check_chip_id(this, BQ25186_REG_MASK_ID, 0x01, 0x0F))
    return;

  // A failed write only sets the warning status: the chip has identified itself, so a later
  // apply_configuration may retry
  if (this->configure_on_boot_)
    this->apply_config(this->regs_);
#ifdef USE_BQ25186_PG_GPO
  if (this->pg_gpo_switch_ != nullptr) {
    // The poll below puts the pin in output mode; without a restore mode the switch takes its level from it
    auto initial = this->pg_gpo_switch_->get_initial_state_with_restore_mode();
    if (initial.has_value())
      this->pg_gpo_switch_->publish_state(*initial != this->pg_gpo_switch_->is_inverted());  // the first poll writes it
  }
#endif
  // The poller first runs after a random share of the interval; the entities get a state now
  this->update();
}

#ifdef USE_BQ25186_PG_GPO
// The pin mode and level come from the switch, so every poll puts back whatever the chip lost
void BQ25186Component::check_pg_gpo_(const BQ25186StatusRegisters &status, bool &pending) {
  if (this->pg_gpo_switch_ == nullptr)
    return;
  if ((status[BQ25186_REG_VBAT_CTRL] & BQ25186_VBAT_CTRL_PG_MODE) == 0 &&
      !this->update_register_(BQ25186_REG_VBAT_CTRL, BQ25186_VBAT_CTRL_PG_MODE, BQ25186_VBAT_CTRL_PG_MODE)) {
    this->status_set_warning(LOG_STR("PG/GPO pin not in output mode"));
    pending = true;
  }
  const bool level = (status[BQ25186_REG_SYS_REG] & BQ25186_SYS_REG_PG_GPO_LEVEL) != 0;
  if (!this->pg_gpo_switch_->has_state()) {
    this->pg_gpo_switch_->publish_state(level);
    return;
  }
  // state is what the switch shows; an inverted switch drives the opposite level
  const bool wanted = this->pg_gpo_switch_->state != this->pg_gpo_switch_->is_inverted();
  if (level != wanted && !this->write_pg_gpo_level(wanted)) {
    this->status_set_warning(LOG_STR("PG/GPO level write failed"));
    pending = true;
  }
}
#endif

// Every poll also serves the I2C watchdog, which wants to see traffic within its period
void BQ25186Component::update() {
  BQ25186StatusRegisters status;
  if (!this->read_bytes(BQ25186_REG_STAT0, status.data(), status.size())) {
    this->status_set_warning(LOG_STR("Status read failed"));
    return;
  }
  // Whatever failed is tried again here, so the warning only stays while something is still wrong
  bool pending = false;
  // A watchdog or hardware reset puts the power-on values back, and a failed write leaves a register
  // behind; either way the configuration is written again
  if (this->configured_ && !this->config_matches_(status)) {
    ESP_LOGW(TAG, "Configuration registers differ from the values applied, writing them again");
    if (this->apply_config(this->regs_)) {
      status[BQ25186_REG_VBAT_CTRL] |= BQ25186_VBAT_CTRL_PG_MODE;  // part of the write when a switch is configured
    } else {
      pending = true;
    }
  }
#ifdef USE_BQ25186_PG_GPO
  this->check_pg_gpo_(status, pending);
#endif
  if (!pending)
    this->status_clear_warning();
#ifdef BQ25186_BINARY_SENSOR_COUNT
  for (auto *sensor : this->binary_sensors_)
    sensor->on_status(status);
#endif
#ifdef BQ25186_TEXT_SENSOR_COUNT
  for (auto *sensor : this->text_sensors_)
    sensor->on_status(status);
#endif
}

void BQ25186Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BQ25186:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
#ifdef USE_BQ25186_PG_GPO
  LOG_SWITCH("  ", "PG/GPO", this->pg_gpo_switch_);
#endif
}

}  // namespace esphome::bq25186
