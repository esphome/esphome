#include "as734x.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

#include <algorithm>

#ifdef USE_AS7341
#include "as7341.h"
#endif

#ifdef USE_AS7343
#include "as7343.h"
#endif

namespace esphome::as734x {

static const char *const TAG = "as734x";

static constexpr uint32_t MIN_COLLECTION_TIMEOUT_MS = 30 * 1000;
static constexpr uint8_t COLLECTION_TIMEOUT_MARGIN = 2;

namespace {

float integration_time_ms(uint8_t atime, uint16_t astep) { return (1.0f + atime) * (1.0f + astep) * 2.78e-3f; }
float gain_multiplier(Gain gain) { return gain == GAIN_0_5X ? 0.5f : static_cast<float>(1 << (gain - 1)); }

}  // namespace

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

  if (!this->device_->write_default_config() || !this->device_->write_atime(this->atime_) ||
      !this->device_->write_astep(this->astep_) || !this->device_->write_gain(this->gain_)) {
    ESP_LOGE(TAG, "Configuration failed");
    this->mark_failed();
    return;
  }

  this->state_ = State::IDLE;
  this->disable_loop();
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
                "  Gain: %gx\n"
                "  ATIME: %u\n"
                "  ASTEP: %u",
                this->model_ == Model::AS7341 ? "AS7341" : "AS7343", gain_multiplier(this->gain_), this->atime_,
                this->astep_);
}

void AS734XComponent::update() {
  if (!this->is_ready()) {
    return;
  }
  if (this->state_ == State::IDLE) {
    ESP_LOGV(TAG, "Initiating new data collection");
    this->state_ = State::START_MEASUREMENT;
    this->enable_loop();
  } else {
    ESP_LOGW(TAG, "Skipping update, previous measurement still running");
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
      this->readings_.timeout_ms =
          std::max(MIN_COLLECTION_TIMEOUT_MS,
                   static_cast<uint32_t>(COLLECTION_TIMEOUT_MARGIN * this->device_->get_number_of_smux_steps() *
                                         this->device_->get_integration_cycles() *
                                         integration_time_ms(this->atime_, this->astep_)));
      this->device_->write_atime(this->atime_);
      this->device_->write_astep(this->astep_);
      this->device_->write_gain(this->gain_);
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
      } else if (millis() - this->readings_.millis_start > this->readings_.timeout_ms) {
        this->abort_measurement_("SMUX configuration timeout");
      }
      break;

    case State::READ_DATA:
      ESP_LOGVV(TAG, "READ_DATA");
      if (this->device_->is_data_ready()) {
        bool device_saturated = false;
        if (!this->device_->read_channels(this->readings_.smux_step, this->readings_.raw_counts, device_saturated)) {
          this->abort_measurement_("Failed to read channel data");
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
          this->state_ = State::CONFIGURE_SMUX;
        }
      } else if (millis() - this->readings_.millis_start > this->readings_.timeout_ms) {
        this->abort_measurement_("Data collection timeout");
      }
      break;

    case State::READY_TO_PUBLISH:
      ESP_LOGVV(TAG, "READY_TO_PUBLISH");
      this->publish_channel_readings_();
      this->status_clear_warning();
      this->state_ = State::IDLE;
      this->disable_loop();
      break;
  }
}

void AS734XComponent::abort_measurement_(const char *reason) {
  ESP_LOGW(TAG, "%s", reason);
  this->device_->enable_spectral_measurement(false);
  this->status_set_warning(reason);
  this->state_ = State::IDLE;
  this->disable_loop();
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

bool AS734xBase::write_gain(Gain gain) { return this->write_byte_(this->registers().cfg1, gain); }
bool AS734xBase::write_atime(uint8_t atime) { return this->write_byte_(this->registers().atime, atime); }
bool AS734xBase::write_astep(uint16_t astep) {  // ASTEP is above 0x80 on both parts
  return this->i2c_device_->write_byte_16(this->registers().astep, this->swap_bytes_(astep));
}

bool AS734xBase::enable_power(bool enable) {
  return this->write_register_bit_(this->registers().enable, enable, this->registers().enable_pon_bit);
}

bool AS734xBase::enable_spectral_measurement(bool enable) {
  return this->write_register_bit_(this->registers().enable, enable, this->registers().enable_sp_en_bit);
}

bool AS734xBase::enable_smux() {
  return this->set_register_bit_(this->registers().enable, this->registers().enable_smux_en_bit);
}

bool AS734xBase::is_smux_busy() {
  bool busy = true;  // a failed read reads as busy, so the caller times out instead of moving on
  this->read_register_bit_(this->registers().enable, this->registers().enable_smux_en_bit, busy);
  return busy;
}

bool AS734xBase::is_data_ready() {
  bool ready = false;  // a failed read reads as not ready
  this->read_register_bit_(this->registers().status2, this->registers().status2_avalid_bit, ready);
  return ready;
}

bool AS734xBase::select_low_bank_(bool low) {  // CFG0 is outside the low window, so reach it directly
  const uint8_t mask = 1 << this->registers().cfg0_reg_bank_bit;
  uint8_t data{0};
  if (!this->i2c_device_->read_byte(this->registers().cfg0, &data)) {
    ESP_LOGW(TAG, "Could not read CFG0, register bank left unchanged");
    return false;
  }
  data = low ? (data | mask) : (data & ~mask);
  if (!this->i2c_device_->write_byte(this->registers().cfg0, data)) {
    ESP_LOGW(TAG, "Could not write CFG0, register bank left unchanged");
    return false;
  }
  return true;
}

bool AS734xBase::read_byte_(uint8_t address, uint8_t *value) {
  if (!this->needs_low_bank_(address)) {
    return this->i2c_device_->read_byte(address, value);
  }
  if (!this->select_low_bank_(true)) {
    return false;
  }
  const bool ok = this->i2c_device_->read_byte(address, value);
  return this->select_low_bank_(false) && ok;
}

bool AS734xBase::write_byte_(uint8_t address, uint8_t value) {
  if (!this->needs_low_bank_(address)) {
    return this->i2c_device_->write_byte(address, value);
  }
  if (!this->select_low_bank_(true)) {
    return false;
  }
  const bool ok = this->i2c_device_->write_byte(address, value);
  return this->select_low_bank_(false) && ok;
}

bool AS734xBase::read_register_bit_(uint8_t address, uint8_t bit_position, bool &bit_value) {
  uint8_t data{0};
  if (!this->read_byte_(address, &data)) {
    ESP_LOGW(TAG, "Read of register 0x%02X failed", address);
    return false;
  }
  bit_value = (data & (1 << bit_position)) != 0;
  return true;
}

bool AS734xBase::write_register_bit_(uint8_t address, bool value, uint8_t bit_position) {
  return value ? this->set_register_bit_(address, bit_position) : this->clear_register_bit_(address, bit_position);
}

bool AS734xBase::set_register_bit_(uint8_t address, uint8_t bit_position) {
  return this->update_register_bit_(address, bit_position, true);
}

bool AS734xBase::clear_register_bit_(uint8_t address, uint8_t bit_position) {
  return this->update_register_bit_(address, bit_position, false);
}

bool AS734xBase::update_register_bit_(uint8_t address, uint8_t bit_position, bool value) {
  uint8_t data{0};
  if (!this->read_byte_(address, &data)) {
    ESP_LOGW(TAG, "Read of register 0x%02X failed, not writing bit %u", address, bit_position);
    return false;
  }
  const uint8_t mask = 1 << bit_position;
  data = value ? (data | mask) : (data & ~mask);
  return this->write_byte_(address, data);
}

}  // namespace esphome::as734x
