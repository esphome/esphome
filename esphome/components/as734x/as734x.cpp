#include "as734x.h"
#include "color_helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

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
// The gain enum counts in powers of two starting at 0.5x, so the multiplier is 2^gain / 2.
float gain_multiplier(Gain gain) { return static_cast<float>(1 << static_cast<uint8_t>(gain)) / 2.0f; }

uint16_t maximum_spectral_adc(uint8_t atime, uint16_t astep) {
  static constexpr uint32_t MAX_ADC_COUNT = 65535;
  const uint32_t value = (atime + 1u) * (astep + 1u);
  return static_cast<uint16_t>(std::min(value, MAX_ADC_COUNT));
}

const char *model_name(Model model) {
  switch (model) {
    case Model::AS7341:
      return "AS7341";
    case Model::TCS3448:
      return "TCS3448";
    default:
      return "AS7343";
  }
}

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
    case Model::TCS3448:
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
                model_name(this->model_), gain_multiplier(this->gain_), this->atime_, this->astep_);
  ESP_LOGCONFIG(TAG, "  Glass attenuation factor: %.3f", this->glass_attenuation_factor_);
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Saturation level", this->saturation_level_sensor_);
  LOG_SENSOR("  ", "Illuminance", this->illuminance_sensor_);
  LOG_SENSOR("  ", "Irradiance photopic", this->irradiance_photopic_sensor_);
  LOG_SENSOR("  ", "Irradiance PAR", this->irradiance_par_sensor_);
  LOG_SENSOR("  ", "PPFD", this->ppfd_sensor_);
  LOG_SENSOR("  ", "Color temperature", this->color_temperature_sensor_);
#endif
#ifdef USE_AS734X_RGB
  LOG_TEXT_SENSOR("  ", "RGB hex", this->rgb_hex_sensor_);
#endif
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
      if (!this->device_->write_atime(this->atime_) || !this->device_->write_astep(this->astep_) ||
          !this->device_->write_gain(this->gain_)) {
        this->abort_measurement_("Failed to apply measurement settings");
        break;
      }
      // Remember what the measurement ran with, so the maths below matches the data even if the
      // configuration is changed while a measurement is in flight.
      this->readings_.gain = this->gain_;
      this->readings_.atime = this->atime_;
      this->readings_.astep = this->astep_;
      this->readings_.smux_step = 0;
      this->state_ = State::CONFIGURE_SMUX;
      break;

    case State::CONFIGURE_SMUX:
      ESP_LOGVV(TAG, "CONFIGURE_SMUX");
      this->device_->enable_spectral_measurement(false);
      delay(5);
      if (!this->device_->prepare_for_smux_step(this->readings_.smux_step)) {
        this->abort_measurement_("Failed to configure SMUX");
        break;
      }
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
          this->state_ = State::CALCULATE;
        } else {
          this->state_ = State::CONFIGURE_SMUX;
        }
      } else if (millis() - this->readings_.millis_start > this->readings_.timeout_ms) {
        this->abort_measurement_("Data collection timeout");
      }
      break;

    case State::CALCULATE:
      ESP_LOGVV(TAG, "CALCULATE");
      this->calculate_basic_counts_();
      this->calculate_saturation_level_();
      this->calculate_light_metrics_();
      this->calculate_color_();
      this->state_ = State::READY_TO_PUBLISH;
      break;

    case State::READY_TO_PUBLISH:
      ESP_LOGVV(TAG, "READY_TO_PUBLISH");
      this->publish_channel_readings_();
      this->publish_basic_counts_();
      this->publish_light_metrics_();
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

void AS734XComponent::enable_led(bool enable) {
  if (this->device_ != nullptr) {
    this->device_->enable_led(enable);
  }
}

// A basic count is the raw count scaled to one unit of gain and one millisecond of integration
// time, which makes readings taken with different settings comparable. The manufacturer's
// conversion factors are calibrated against that definition.
void AS734XComponent::calculate_basic_counts_() {
  this->calculated_.basic_counts.fill(0.0f);
  this->calculated_.max_basic_count = 0.0f;
  this->calculated_.max_band_basic_count = 0.0f;
  this->calculated_.clear_basic_count = 0.0f;

  const float gain_x = gain_multiplier(this->readings_.gain);
  const float t_int_ms = integration_time_ms(this->readings_.atime, this->readings_.astep);
  if (gain_x <= 0.0f || t_int_ms <= 0.0f) {
    return;
  }
  const float inv_exposure = 1.0f / (gain_x * t_int_ms);

  for (uint8_t i = 0; i < this->device_->get_number_of_channels(); i++) {
    float basic_count = this->readings_.raw_counts[i] * inv_exposure;

    basic_count -= this->dark_current_[i];
    basic_count = std::max(basic_count, 0.0f);
    basic_count *= this->device_->get_gain_correction(i, this->readings_.gain);
    basic_count *= this->channel_correction_[i];
    basic_count *= this->glass_attenuation_factor_;

    this->calculated_.basic_counts[i] = basic_count;
    this->calculated_.max_basic_count = std::max(this->calculated_.max_basic_count, basic_count);

    const uint16_t wavelength = this->device_->get_channel_wavelength(i);
    if (wavelength == WIDEBAND_NM) {
      this->calculated_.clear_basic_count = basic_count;
    } else if (wavelength >= VISIBLE_MIN_NM && wavelength <= VISIBLE_MAX_NM) {
      this->calculated_.max_band_basic_count = std::max(this->calculated_.max_band_basic_count, basic_count);
    }
  }
}

float AS734XComponent::normalization_divisor_() const {
  float divisor = 1.0f;
  switch (this->normalization_) {
    case Normalization::ALL:
      divisor = this->calculated_.max_basic_count;
      break;
    case Normalization::BANDS:
      divisor = this->calculated_.max_band_basic_count;
      break;
    case Normalization::CLEAR:
      divisor = this->calculated_.clear_basic_count;
      break;
    case Normalization::NONE:
      return 1.0f;
  }
  // In the dark the reference can be zero, and dividing by that would publish infinities.
  return divisor > 0.0f ? divisor : 1.0f;
}

// Each channel contributes a fixed amount per basic count to every integrated quantity, so the
// totals are a weighted sum over the channels.
void AS734XComponent::calculate_light_metrics_() {
  float irradiance_photopic = 0.0f;
  float irradiance_par = 0.0f;
  float ppfd = 0.0f;

  for (uint8_t i = 0; i < this->device_->get_number_of_channels(); i++) {
    const float basic_count = this->calculated_.basic_counts[i];
    const ChannelContribution contribution = this->device_->get_channel_contribution(i);

    irradiance_photopic += contribution.irradiance_photopic * basic_count;
    irradiance_par += contribution.irradiance_par * basic_count;
    ppfd += contribution.ppfd * basic_count;
  }

  this->calculated_.irradiance_photopic = irradiance_photopic;
  this->calculated_.irradiance_par = irradiance_par;
  this->calculated_.ppfd = ppfd * 1e3f;
  this->calculated_.illuminance = irradiance_photopic * LUMENS_PER_WATT;

  ESP_LOGV(TAG, "Photopic irradiance %.4f, PAR %.4f, PPFD %.4f, illuminance %.2f lx",
           this->calculated_.irradiance_photopic, this->calculated_.irradiance_par, this->calculated_.ppfd,
           this->calculated_.illuminance);
}

void AS734XComponent::calculate_color_() {
  float tri_x = 0.0f;
  float tri_y = 0.0f;
  float tri_z = 0.0f;

  for (uint8_t i = 0; i < this->device_->get_number_of_channels(); i++) {
    const float basic_count = this->calculated_.basic_counts[i];
    const ChannelTristimulus tristimulus = this->device_->get_channel_tristimulus(i);

    tri_x += tristimulus.x * basic_count;
    tri_y += tristimulus.y * basic_count;
    tri_z += tristimulus.z * basic_count;
  }

  this->calculated_.color_temperature = tristimulus_to_cct(tri_x, tri_y, tri_z);
  ESP_LOGV(TAG, "XYZ %.4f, %.4f, %.4f -> CCT %.0f K", tri_x, tri_y, tri_z, this->calculated_.color_temperature);

#ifdef USE_AS734X_RGB
  // Convert to chromaticity first, so the colour describes the hue rather than the brightness.
  // Absolute tristimulus values clamp to white under anything but dim light.
  const float sum = tri_x + tri_y + tri_z;
  if (sum > MIN_TRISTIMULUS_SUM) {
    uint8_t r, g, b;
    tristimulus_to_rgb(tri_x / sum, tri_y / sum, tri_z / sum, r, g, b);
    snprintf(this->rgb_hex_, sizeof(this->rgb_hex_), "%02x%02x%02x", r, g, b);
  } else {
    // No usable chromaticity, so report black rather than leaving the previous colour in place or
    // publishing an empty string.
    strncpy(this->rgb_hex_, "000000", sizeof(this->rgb_hex_));
  }
#endif
}

void AS734XComponent::calculate_saturation_level_() {
  const uint16_t max_adc = maximum_spectral_adc(this->readings_.atime, this->readings_.astep);
  const uint16_t scanned = *std::max_element(
      this->readings_.raw_counts.begin(), this->readings_.raw_counts.begin() + this->device_->get_number_of_channels());
  // The AS7343 publishes the mean of its two clear cycles, which can only sit below a saturated
  // reading, so the peak the device saw is taken into account as well.
  const uint16_t highest = std::max(scanned, this->device_->get_peak_raw_count());
  this->calculated_.saturation_level = (max_adc == 0) ? 0.0f : 100.0f * highest / max_adc;
  ESP_LOGV(TAG, "Highest ADC count %u of %u (%.1f%%)", highest, max_adc, this->calculated_.saturation_level);
}

#ifdef USE_SENSOR
void AS734XComponent::publish_channel_readings_() {
  for (uint8_t i = 0; i < this->device_->get_number_of_channels(); i++) {
    if (this->band_counts_sensors_[i] != nullptr) {
      this->band_counts_sensors_[i]->publish_state(this->readings_.raw_counts[i]);
    }
  }
  if (this->saturation_level_sensor_ != nullptr) {
    this->saturation_level_sensor_->publish_state(this->calculated_.saturation_level);
  }
}

void AS734XComponent::publish_basic_counts_() {
  // Normalization describes the shape of the spectrum rather than its absolute level. Channels
  // outside the chosen reference can exceed 1.0 and are left that way rather than clipped: with
  // BANDS the near infrared channel legitimately sits well above the visible bands.
  const float scale = 1.0f / this->normalization_divisor_();
  for (uint8_t i = 0; i < this->device_->get_number_of_channels(); i++) {
    if (this->band_basic_counts_sensors_[i] != nullptr) {
      this->band_basic_counts_sensors_[i]->publish_state(this->calculated_.basic_counts[i] * scale);
    }
  }
}
#else
void AS734XComponent::publish_channel_readings_() {}
void AS734XComponent::publish_basic_counts_() {}
#endif

void AS734XComponent::publish_light_metrics_() {
#ifdef USE_SENSOR
  if (this->illuminance_sensor_ != nullptr) {
    this->illuminance_sensor_->publish_state(this->calculated_.illuminance);
  }
  if (this->irradiance_photopic_sensor_ != nullptr) {
    this->irradiance_photopic_sensor_->publish_state(this->calculated_.irradiance_photopic);
  }
  if (this->irradiance_par_sensor_ != nullptr) {
    this->irradiance_par_sensor_->publish_state(this->calculated_.irradiance_par);
  }
  if (this->ppfd_sensor_ != nullptr) {
    this->ppfd_sensor_->publish_state(this->calculated_.ppfd);
  }
  if (this->color_temperature_sensor_ != nullptr) {
    this->color_temperature_sensor_->publish_state(this->calculated_.color_temperature);
  }
#endif
#ifdef USE_AS734X_RGB
  if (this->rgb_hex_sensor_ != nullptr) {
    this->rgb_hex_sensor_->publish_state(this->rgb_hex_);
  }
#endif
}

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

bool AS734xBase::enable_led(bool enable) {
  return this->write_register_bit_(this->registers().led, enable, this->registers().led_act_bit);
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
