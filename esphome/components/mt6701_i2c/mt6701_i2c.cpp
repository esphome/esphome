#include "mt6701_i2c.h"
#include "esphome/core/log.h"

namespace esphome::mt6701_i2c {

static const char *const TAG = "mt6701_i2c";

// Registers touched by apply_config_(), each read-modify-written at most once.
static const uint8_t CONFIG_REGS[] = {
    REG_ABZ_MUX_DIR, REG_RES_H, REG_ABZ_RES_L, REG_CONFIG_H,  REG_ZERO_L,
    REG_HYST_L,      REG_OUT,   REG_A_HIGH,    REG_A_START_L, REG_A_STOP_L,
};
static const uint8_t CONFIG_REG_COUNT = sizeof(CONFIG_REGS);

void MT6701I2CComponent::setup() {
  ESP_LOGCONFIG(TAG, "Running setup");
  // Probe the device by reading the angle registers.
  uint16_t count;
  if (!this->read_count(count)) {
    ESP_LOGE(TAG, "MT6701 not responding on I2C");
    this->mark_failed();
    return;
  }
  this->apply_config_();
}

void MT6701I2CComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MT6701 (I2C):");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    return;
  }
  if (this->direction_.has_value())
    ESP_LOGCONFIG(TAG, "  Direction bit: %u", *this->direction_);
  if (this->zero_offset_.has_value())
    ESP_LOGCONFIG(TAG, "  Zero offset: %u", *this->zero_offset_);
  if (this->hysteresis_.has_value())
    ESP_LOGCONFIG(TAG, "  Hysteresis code: %u", *this->hysteresis_);
  if (this->output_mode_uvw_.has_value())
    ESP_LOGCONFIG(TAG, "  Output mode: %s", *this->output_mode_uvw_ ? "UVW" : "ABZ");
  if (this->abz_ppr_.has_value())
    ESP_LOGCONFIG(TAG, "  ABZ pulses/rev: %u", *this->abz_ppr_);
  if (this->z_pulse_width_.has_value())
    ESP_LOGCONFIG(TAG, "  Z pulse width code: %u", *this->z_pulse_width_);
  if (this->uvw_pole_pairs_.has_value())
    ESP_LOGCONFIG(TAG, "  UVW pole pairs: %u", *this->uvw_pole_pairs_);
  if (this->out_pin_pwm_.has_value())
    ESP_LOGCONFIG(TAG, "  Output pin mode: %s", *this->out_pin_pwm_ ? "PWM" : "analog");
  if (this->pwm_freq_.has_value())
    ESP_LOGCONFIG(TAG, "  PWM frequency code: %u", *this->pwm_freq_);
  if (this->pwm_pol_.has_value())
    ESP_LOGCONFIG(TAG, "  PWM polarity code: %u", *this->pwm_pol_);
  if (this->analog_start_.has_value())
    ESP_LOGCONFIG(TAG, "  Analog start: %u", *this->analog_start_);
  if (this->analog_stop_.has_value())
    ESP_LOGCONFIG(TAG, "  Analog stop: %u", *this->analog_stop_);
}

bool MT6701I2CComponent::read_count(uint16_t &count) {
  // Burst read both angle registers so the two bytes are consistent.
  uint8_t data[2];
  if (this->read_register(REG_ANGLE_H, data, 2) != i2c::ERROR_OK)
    return false;
  // 14-bit angle: D[13:6] in data[0], D[5:0] in the top 6 bits of data[1].
  count = encode_uint16(data[0], data[1]) >> 2;
  return true;
}

bool MT6701I2CComponent::update_register_(uint8_t reg, uint8_t mask, uint8_t value) {
  uint8_t current;
  if (this->read_register(reg, &current, 1) != i2c::ERROR_OK)
    return false;
  uint8_t updated = (current & ~mask) | (value & mask);
  if (updated == current)
    return true;
  return this->write_register(reg, &updated, 1) == i2c::ERROR_OK;
}

void MT6701I2CComponent::apply_config_() {
  // Collect the configured bits per register first, so registers shared by
  // several options (e.g. 0x32 holds hysteresis, Z width and zero offset bits)
  // are read-modify-written only once.
  uint8_t masks[CONFIG_REG_COUNT] = {};
  uint8_t values[CONFIG_REG_COUNT] = {};
  auto add = [&masks, &values](uint8_t reg, uint8_t mask, uint8_t value) {
    for (uint8_t i = 0; i < CONFIG_REG_COUNT; i++) {
      if (CONFIG_REGS[i] == reg) {
        masks[i] |= mask;
        values[i] |= value & mask;
        return;
      }
    }
  };

  if (this->direction_.has_value())
    add(REG_ABZ_MUX_DIR, 0x02, *this->direction_ << 1);
  if (this->output_mode_uvw_.has_value())
    add(REG_ABZ_MUX_DIR, 0x40, *this->output_mode_uvw_ ? 0x40 : 0x00);
  if (this->zero_offset_.has_value()) {
    add(REG_CONFIG_H, 0x0F, *this->zero_offset_ >> 8);
    add(REG_ZERO_L, 0xFF, *this->zero_offset_ & 0xFF);
  }
  if (this->hysteresis_.has_value()) {
    add(REG_CONFIG_H, 0x80, (*this->hysteresis_ & 0x04) << 5);  // HYST[2] -> bit7
    add(REG_HYST_L, 0xC0, (*this->hysteresis_ & 0x03) << 6);    // HYST[1:0] -> bits7:6
  }
  if (this->abz_ppr_.has_value()) {
    // ABZ_RES is stored as (pulses per revolution - 1), 10 bits.
    uint16_t res = *this->abz_ppr_ - 1;
    add(REG_RES_H, 0x03, res >> 8);
    add(REG_ABZ_RES_L, 0xFF, res & 0xFF);
  }
  if (this->z_pulse_width_.has_value())
    add(REG_CONFIG_H, 0x70, (*this->z_pulse_width_ & 0x07) << 4);
  if (this->uvw_pole_pairs_.has_value()) {
    // UVW_RES is stored as (pole pairs - 1), 4 bits, in the high nibble.
    add(REG_RES_H, 0xF0, (*this->uvw_pole_pairs_ - 1) << 4);
  }
  if (this->out_pin_pwm_.has_value())
    add(REG_OUT, 0x20, *this->out_pin_pwm_ ? 0x20 : 0x00);
  if (this->pwm_freq_.has_value())
    add(REG_OUT, 0x80, (*this->pwm_freq_ & 0x01) << 7);
  if (this->pwm_pol_.has_value())
    add(REG_OUT, 0x40, (*this->pwm_pol_ & 0x01) << 6);
  if (this->analog_start_.has_value()) {
    add(REG_A_HIGH, 0x0F, *this->analog_start_ >> 8);
    add(REG_A_START_L, 0xFF, *this->analog_start_ & 0xFF);
  }
  if (this->analog_stop_.has_value()) {
    add(REG_A_HIGH, 0xF0, (*this->analog_stop_ >> 8) << 4);
    add(REG_A_STOP_L, 0xFF, *this->analog_stop_ & 0xFF);
  }

  bool ok = true;
  for (uint8_t i = 0; i < CONFIG_REG_COUNT; i++) {
    if (masks[i] != 0)
      ok &= this->update_register_(CONFIG_REGS[i], masks[i], values[i]);
  }
  if (!ok) {
    // The chip may now be half-configured; surface it instead of just logging.
    this->status_set_warning("configuration register write failed");
  }
}

void MT6701I2CComponent::save_eeprom() {
  if (this->is_failed()) {
    // Timeouts are not delivered to failed components, so starting the
    // sequence here would leave the bus suspended forever.
    ESP_LOGW(TAG, "Not programming EEPROM: component is marked failed");
    return;
  }
  if (this->suspend_sampling_) {
    ESP_LOGW(TAG, "EEPROM programming already in progress");
    return;
  }
  ESP_LOGW(TAG, "Programming MT6701 EEPROM. The EEPROM has a limited number of write "
                "cycles, so avoid doing this repeatedly. Supply must be 4.5-5.5 V.");
  this->suspend_sampling_ = true;

  bool ok = this->write_byte(REG_EEPROM_KEY, EEPROM_KEY_VALUE) && this->write_byte(REG_EEPROM_CMD, EEPROM_CMD_VALUE);
  if (!ok) {
    ESP_LOGE(TAG, "Failed to start EEPROM programming");
    this->suspend_sampling_ = false;
    return;
  }

  this->set_timeout("eeprom", EEPROM_PROGRAM_DELAY_MS, [this]() {
    this->suspend_sampling_ = false;
    ESP_LOGI(TAG, "EEPROM programming complete");
  });
}

}  // namespace esphome::mt6701_i2c
