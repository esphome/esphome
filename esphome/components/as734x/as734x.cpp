#include "as734x.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

#ifdef USE_AS7341
#include "as7341.h"
#endif

#ifdef USE_AS7343
#include "as7343.h"
#endif

namespace esphome::as734x {

static const char *const TAG = "as734x";

static constexpr uint32_t DATA_COLLECTION_TIMEOUT_MS = 30 * 1000;  // 30 seconds

void AS734XComponent::setup_model(Model model) {
  this->model_ = model;

  switch (this->model_) {
#ifdef USE_AS7341
    case Model::AS7341:
      this->device_ = new AS7341(this);  // NOLINT(cppcoreguidelines-owning-memory)
      break;
#endif
#ifdef USE_AS7343
    case Model::AS7343:
      this->device_ = new AS7343(this);  // NOLINT(cppcoreguidelines-owning-memory)
      break;
#endif
    default:
      ESP_LOGE(TAG, "Unknown model");
  }
}

void AS734XComponent::setup() {
  if (this->device_ == nullptr) {
    this->mark_failed();
    return;
  }

  if (!this->device_->verify_device_id()) {
    ESP_LOGE(TAG, "Invalid chip ID");
    this->mark_failed();
    return;
  }

  this->device_->enable_power(false);
  delay(10);  // wait for power off
  if (!this->device_->enable_power(true)) {
    ESP_LOGE(TAG, "Power on failed");
    this->mark_failed();
    return;
  }
  delay(10);  // wait for power on

  this->device_->write_default_config();
  this->device_->write_atime(this->atime_);
  this->device_->write_astep(this->astep_);
  this->device_->write_gain(this->gain_);

  this->state_ = State::IDLE;
}

void AS734XComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "AS734x:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with AS734x failed");
  }
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG,
                "  Model: %s\n"
                "  Gain: %u\n"
                "  ATIME: %u\n"
                "  ASTEP: %u",
                this->model_ == Model::AS7341 ? "AS7341" : "AS7343", this->gain_, this->atime_, this->astep_);
}

void AS734XComponent::update() {
  if (this->is_ready() && this->state_ == State::IDLE) {
    ESP_LOGV(TAG, "Initiating new data collection");
    this->state_ = State::START_MEASUREMENT;
  } else {
    ESP_LOGV(TAG, "Can't initiate new data collection - component not ready");
  }
}

void AS734XComponent::loop() {
  if (!this->is_ready()) {
    return;
  }

  switch (this->state_) {
    case State::NOT_INITIALIZED:
      // we shall not be here
      ESP_LOGE(TAG, "State machine not initialized");
      this->mark_failed();
      break;

    case State::IDLE:
      break;

    case State::START_MEASUREMENT:
      ESP_LOGVV(TAG, "START_MEASUREMENT");
      this->readings_.millis_start = millis();
      this->device_->write_atime(this->atime_);
      this->device_->write_astep(this->astep_);
      this->device_->write_gain(this->gain_);
      this->readings_.first_run = true;
      this->readings_.smux_step = 0;
      this->state_ = State::CONFIGURE_SMUX;
      break;

    case State::CONFIGURE_SMUX:
      ESP_LOGVV(TAG, "CONFIGURE_SMUX");
      this->device_->enable_spectral_measurement(false);
      delay(5);
      this->device_->prepare_for_smux_step(this->readings_.smux_step);
      this->state_ = State::WAIT_SMUX;
      break;

    case State::WAIT_SMUX:
      ESP_LOGVV(TAG, "WAIT_SMUX");
      if (!this->device_->is_smux_busy()) {
        this->device_->enable_spectral_measurement(true);
        this->state_ = State::READ_DATA;
      } else if (millis() - this->readings_.millis_start > DATA_COLLECTION_TIMEOUT_MS) {
        ESP_LOGW(TAG, "SMUX configuration timeout");
        this->state_ = State::IDLE;
      }
      break;

    case State::READ_DATA:
      ESP_LOGVV(TAG, "READ_DATA");
      if (this->device_->is_data_ready()) {
        bool device_saturated = false;
        if (!this->device_->read_channels(this->readings_.smux_step, this->readings_.raw_counts, this->readings_.gain,
                                          device_saturated)) {
          ESP_LOGW(TAG, "Failed to read channel data, aborting measurement");
          this->state_ = State::IDLE;
          break;
        }
        if (this->readings_.first_run) {
          this->readings_.first_run = false;
          ESP_LOGVV(TAG, "Discarding first reading");
          break;
        }
        if (device_saturated) {
          ESP_LOGV(TAG, "Latched data affected by saturation");
        }
        ++this->readings_.smux_step;
        if (this->readings_.smux_step == this->device_->get_number_of_smux_steps()) {
          this->device_->enable_spectral_measurement(false);
          this->state_ = State::READY_TO_PUBLISH;
        } else {
          this->readings_.first_run = true;
          this->state_ = State::CONFIGURE_SMUX;
        }
      } else if (millis() - this->readings_.millis_start > DATA_COLLECTION_TIMEOUT_MS) {
        ESP_LOGW(TAG, "Data collection timeout");
        this->state_ = State::IDLE;
      }
      break;

    case State::READY_TO_PUBLISH:
      ESP_LOGVV(TAG, "READY_TO_PUBLISH");
      this->publish_channel_readings_();
      this->state_ = State::IDLE;
      break;
  }
}

#ifdef USE_SENSOR
void AS734XComponent::publish_channel_readings_() {
  for (uint8_t i = 0; i < this->device_->get_number_of_channels(); i++) {
    if (this->band_counts_sensors_[i] != nullptr) {
      this->band_counts_sensors_[i]->publish_state(this->readings_.raw_counts[i]);
    }
  }
}
#else
void AS734XComponent::publish_channel_readings_() {}
#endif

AS734xBase::AS734xBase(i2c::I2CDevice *i2c_device, uint8_t number_of_channels)
    : i2c_device_(i2c_device), number_of_channels_(number_of_channels) {}

bool AS734xBase::write_gain(Gain gain) { return this->i2c_device_->write_byte(this->registers().CFG1, gain); }
bool AS734xBase::write_atime(uint8_t atime) { return this->i2c_device_->write_byte(this->registers().ATIME, atime); }
bool AS734xBase::write_astep(uint16_t astep) {
  return this->i2c_device_->write_byte_16(this->registers().ASTEP, this->swap_bytes_(astep));
}

bool AS734xBase::enable_power(bool enable) {
  return this->write_register_bit_(this->registers().ENABLE, enable, this->registers().ENABLE_PON_BIT);
}

bool AS734xBase::enable_spectral_measurement(bool enable) {
  return this->write_register_bit_(this->registers().ENABLE, enable, this->registers().ENABLE_SP_EN_BIT);
}

bool AS734xBase::enable_smux() {
  return this->set_register_bit_(this->registers().ENABLE, this->registers().ENABLE_SMUX_EN_BIT);
}

bool AS734xBase::is_smux_busy() {
  return this->read_register_bit_(this->registers().ENABLE, this->registers().ENABLE_SMUX_EN_BIT);
}

bool AS734xBase::is_data_ready() {
  return this->read_register_bit_(this->registers().STATUS2, this->registers().STATUS2_AVALID_BIT);
}

void AS734xBase::set_bank_(bool low) {
  if (low == this->bank_low_) {
    return;
  }
  this->bank_low_ = low;

  // CFG0 is written directly here. Going through the bank-aware bit helpers would call
  // set_bank_for_reg_() again and recurse without end, since CFG0 itself sits outside the low bank.
  const uint8_t mask = 1 << this->registers().CFG0_REG_BANK_BIT;
  uint8_t data{0};
  this->i2c_device_->read_byte(this->registers().CFG0, &data);
  data = low ? (data | mask) : (data & ~mask);
  this->i2c_device_->write_byte(this->registers().CFG0, data);
}

void AS734xBase::set_bank_for_reg_(uint8_t reg) {
  this->set_bank_(reg >= this->registers().BANK_LOW_MIN && reg <= this->registers().BANK_LOW_MAX);
}

bool AS734xBase::read_register_bit_(uint8_t address, uint8_t bit_position) {
  this->set_bank_for_reg_(address);

  uint8_t data{0};
  this->i2c_device_->read_byte(address, &data);
  bool bit = (data & (1 << bit_position)) > 0;
  return bit;
}

bool AS734xBase::write_register_bit_(uint8_t address, bool value, uint8_t bit_position) {
  return value ? this->set_register_bit_(address, bit_position) : this->clear_register_bit_(address, bit_position);
}

bool AS734xBase::set_register_bit_(uint8_t address, uint8_t bit_position) {
  this->set_bank_for_reg_(address);
  uint8_t data{0};
  this->i2c_device_->read_byte(address, &data);
  data |= (1 << bit_position);
  return this->i2c_device_->write_byte(address, data);
}

bool AS734xBase::clear_register_bit_(uint8_t address, uint8_t bit_position) {
  this->set_bank_for_reg_(address);
  uint8_t data{0};
  this->i2c_device_->read_byte(address, &data);
  data &= ~(1 << bit_position);
  return this->i2c_device_->write_byte(address, data);
}

}  // namespace esphome::as734x
