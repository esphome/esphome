#include "bl0940.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace bl0940 {

static const char *const TAG = "bl0940";

static const uint8_t BL0940_FULL_PACKET = 0xAA;
static const uint8_t BL0940_PACKET_HEADER = 0x55;  // 0x58 according to en doc but 0x55 in cn doc

static const uint8_t BL0940_REG_I_FAST_RMS_CTRL = 0x10;
static const uint8_t BL0940_REG_MODE = 0x18;
static const uint8_t BL0940_REG_SOFT_RESET = 0x19;
static const uint8_t BL0940_REG_USR_WRPROT = 0x1A;
static const uint8_t BL0940_REG_TPS_CTRL = 0x1B;

static const uint8_t BL0940_INIT[5][5] = {
    // Reset to default
    {BL0940_REG_SOFT_RESET, 0x5A, 0x5A, 0x5A, 0x38},
    // Enable User Operation Write
    {BL0940_REG_USR_WRPROT, 0x55, 0x00, 0x00, 0xF0},
    // 0x0100 = CF_UNABLE energy pulse, AC_FREQ_SEL 50Hz, RMS_UPDATE_SEL 800mS
    {BL0940_REG_MODE, 0x00, 0x10, 0x00, 0x37},
    // 0x47FF = Over-current and leakage alarm on, Automatic temperature measurement, Interval 100mS
    {BL0940_REG_TPS_CTRL, 0xFF, 0x47, 0x00, 0xFE},
    // 0x181C = Half cycle, Fast RMS threshold 6172
    {BL0940_REG_I_FAST_RMS_CTRL, 0x1C, 0x18, 0x00, 0x1B}};

void BL0940::loop() {
  DataPacket buffer;
  if (!this->available()) {
    return;
  }
  if (read_array((uint8_t *) &buffer, sizeof(buffer))) {
    if (this->validate_checksum_(&buffer)) {
      this->received_package_(&buffer);
    }
  } else {
    ESP_LOGW(TAG, "Junk on wire. Throwing away partial message");
    while (read() >= 0)
      ;
  }
}

bool BL0940::validate_checksum_(DataPacket *data) {
  uint8_t checksum = this->read_command_;
  // Whole package but checksum
  uint8_t *raw = (uint8_t *) data;
  for (uint32_t i = 0; i < sizeof(*data) - 1; i++) {
    checksum += raw[i];
  }
  checksum ^= 0xFF;
  if (checksum != data->checksum) {
    ESP_LOGW(TAG, "Invalid checksum! 0x%02X != 0x%02X", checksum, data->checksum);
  }
  return checksum == data->checksum;
}

void BL0940::update() {
  this->flush();
  this->write_byte(this->read_command_);
  this->write_byte(BL0940_FULL_PACKET);
}

void BL0940::setup() {
#ifdef USE_NUMBER
  // add calibration callbacks
  if (this->voltage_calibration_number_ != nullptr) {
    this->voltage_calibration_number_->add_on_state_callback(
        [this](float state) { this->voltage_calibration_callback_(state); });
    if (this->voltage_calibration_number_->has_state()) {
      this->voltage_calibration_callback_(this->voltage_calibration_number_->state);
    }
  }

  if (this->current_calibration_number_ != nullptr) {
    this->current_calibration_number_->add_on_state_callback(
        [this](float state) { this->current_calibration_callback_(state); });
    if (this->current_calibration_number_->has_state()) {
      this->current_calibration_callback_(this->current_calibration_number_->state);
    }
  }

  if (this->power_calibration_number_ != nullptr) {
    this->power_calibration_number_->add_on_state_callback(
        [this](float state) { this->power_calibration_callback_(state); });
    if (this->power_calibration_number_->has_state()) {
      this->power_calibration_callback_(this->power_calibration_number_->state);
    }
  }

  if (this->energy_calibration_number_ != nullptr) {
    this->energy_calibration_number_->add_on_state_callback(
        [this](float state) { this->energy_calibration_callback_(state); });
    if (this->energy_calibration_number_->has_state()) {
      this->energy_calibration_callback_(this->energy_calibration_number_->state);
    }
  }
#endif

  // calculate calibrated reference values
  this->voltage_reference_cal_ = this->voltage_reference_ / this->voltage_cal_;
  this->current_reference_cal_ = this->current_reference_ / this->current_cal_;
  this->power_reference_cal_ = this->power_reference_ / this->power_cal_;
  this->energy_reference_cal_ = this->energy_reference_ / this->energy_cal_;

  for (auto *i : BL0940_INIT) {
    this->write_byte(this->write_command_), this->write_array(i, 5);
    delay(1);
  }
  this->flush();
}

float BL0940::calculate_power_reference_() {
  // calculate power reference based on voltage and current reference
  return this->voltage_reference_cal_ * this->current_reference_cal_ * 4046 / 324004 / 79931;
}

float BL0940::calculate_energy_reference_() {
  // formula: 3600000 * 4046 * RL * R1 * 1000 / (1638.4 * 256) / Vref² / (R1 + R2)
  // or:  power_reference_ * 3600000 / (1638.4 * 256)
  return this->power_reference_cal_ * 3600000 / (1638.4 * 256);
}

float BL0940::calculate_calibration_value_(float state) { return (100 + state) / 100; }

void BL0940::reset_calibration() {
#ifdef USE_NUMBER
  if (this->current_calibration_number_ != nullptr && this->current_cal_ != 1) {
    this->current_calibration_number_->make_call().set_value(0).perform();
  }
  if (this->voltage_calibration_number_ != nullptr && this->voltage_cal_ != 1) {
    this->voltage_calibration_number_->make_call().set_value(0).perform();
  }
  if (this->power_calibration_number_ != nullptr && this->power_cal_ != 1) {
    this->power_calibration_number_->make_call().set_value(0).perform();
  }
  if (this->energy_calibration_number_ != nullptr && this->energy_cal_ != 1) {
    this->energy_calibration_number_->make_call().set_value(0).perform();
  }
#endif
  ESP_LOGD(TAG, "external calibration values restored to initial state");
}

void BL0940::current_calibration_callback_(float state) {
  this->current_cal_ = this->calculate_calibration_value_(state);
  ESP_LOGV(TAG, "update current calibration state: %f", this->current_cal_);
  this->recalibrate_();
}
void BL0940::voltage_calibration_callback_(float state) {
  this->voltage_cal_ = this->calculate_calibration_value_(state);
  ESP_LOGV(TAG, "update voltage calibration state: %f", this->voltage_cal_);
  this->recalibrate_();
}
void BL0940::power_calibration_callback_(float state) {
  this->power_cal_ = this->calculate_calibration_value_(state);
  ESP_LOGV(TAG, "update power calibration state: %f", this->power_cal_);
  this->recalibrate_();
}
void BL0940::energy_calibration_callback_(float state) {
  this->energy_cal_ = this->calculate_calibration_value_(state);
  ESP_LOGV(TAG, "update energy calibration state: %f", this->energy_cal_);
  this->recalibrate_();
}

void BL0940::recalibrate_() {
  ESP_LOGV(TAG, "Recalibrating reference values");
  this->voltage_reference_cal_ = this->voltage_reference_ / this->voltage_cal_;
  this->current_reference_cal_ = this->current_reference_ / this->current_cal_;

  if (this->voltage_cal_ != 1 || this->current_cal_ != 1) {
    this->power_reference_ = this->calculate_power_reference_();
  }
  this->power_reference_cal_ = this->power_reference_ / this->power_cal_;

  if (this->voltage_cal_ != 1 || this->current_cal_ != 1 || this->power_cal_ != 1) {
    this->energy_reference_ = this->calculate_energy_reference_();
  }
  this->energy_reference_cal_ = this->energy_reference_ / this->energy_cal_;

  ESP_LOGD(TAG,
           "Recalibrated reference values:\n"
           "Voltage: %f\n, Current: %f\n, Power: %f\n, Energy: %f\n",
           this->voltage_reference_cal_, this->current_reference_cal_, this->power_reference_cal_,
           this->energy_reference_cal_);
}

float BL0940::update_temp_(sensor::Sensor *sensor, uint16_le_t temperature) const {
  auto tb = (float) temperature;
  float converted_temp = ((float) 170 / 448) * (tb / 2 - 32) - 45;
  if (sensor != nullptr) {
    if (sensor->has_state() && std::abs(converted_temp - sensor->get_state()) > max_temperature_diff_) {
      ESP_LOGD(TAG, "Invalid temperature change. Sensor: '%s', Old temperature: %f, New temperature: %f",
               sensor->get_name().c_str(), sensor->get_state(), converted_temp);
      return 0.0f;
    }
    sensor->publish_state(converted_temp);
  }
  return converted_temp;
}

void BL0940::received_package_(DataPacket *data) {
  // Bad header
  if (data->frame_header != BL0940_PACKET_HEADER) {
    ESP_LOGI(TAG, "Invalid data. Header mismatch: %d", data->frame_header);
    return;
  }

  // cf_cnt is only 24 bits, so track overflows
  uint32_t cf_cnt = (uint24_t) data->cf_cnt;
  cf_cnt |= this->prev_cf_cnt_ & 0xff000000;
  if (cf_cnt < this->prev_cf_cnt_) {
    cf_cnt += 0x1000000;
  }
  this->prev_cf_cnt_ = cf_cnt;

  float v_rms = (uint24_t) data->v_rms / this->voltage_reference_cal_;
  float i_rms = (uint24_t) data->i_rms / this->current_reference_cal_;
  float watt = (int24_t) data->watt / this->power_reference_cal_;
  float total_energy_consumption = cf_cnt / this->energy_reference_cal_;

  float tps1 = update_temp_(this->internal_temperature_sensor_, data->tps1);
  float tps2 = update_temp_(this->external_temperature_sensor_, data->tps2);

  if (this->voltage_sensor_ != nullptr) {
    this->voltage_sensor_->publish_state(v_rms);
  }
  if (this->current_sensor_ != nullptr) {
    this->current_sensor_->publish_state(i_rms);
  }
  if (this->power_sensor_ != nullptr) {
    this->power_sensor_->publish_state(watt);
  }
  if (this->energy_sensor_ != nullptr) {
    this->energy_sensor_->publish_state(total_energy_consumption);
  }

  ESP_LOGV(TAG, "BL0940: U %fV, I %fA, P %fW, Cnt %" PRId32 ", ∫P %fkWh, T1 %f°C, T2 %f°C", v_rms, i_rms, watt, cf_cnt,
           total_energy_consumption, tps1, tps2);
}

void BL0940::dump_config() {  // NOLINT(readability-function-cognitive-complexity)
  ESP_LOGCONFIG(TAG,
                "BL0940:\n"
                "  LEGACY MODE: %s\n"
                "  READ  CMD: 0x%02X\n"
                "  WRITE CMD: 0x%02X\n"
                "  ------------------\n"
                "  Current reference: %f\n"
                "  Energy reference: %f\n"
                "  Power reference: %f\n"
                "  Voltage reference: %f\n",
                TRUEFALSE(this->legacy_mode_enabled_), this->read_command_, this->write_command_,
                this->current_reference_, this->energy_reference_, this->power_reference_, this->voltage_reference_);
#ifdef USE_NUMBER
  ESP_LOGCONFIG(TAG,
                "BL0940:\n"
                "  Current calibration: %f\n"
                "  Energy calibration: %f\n"
                "  Power calibration: %f\n"
                "  Voltage calibration: %f\n",
                this->current_cal_, this->energy_cal_, this->power_cal_, this->voltage_cal_);
#endif
  LOG_SENSOR("", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("", "Current", this->current_sensor_);
  LOG_SENSOR("", "Power", this->power_sensor_);
  LOG_SENSOR("", "Energy", this->energy_sensor_);
  LOG_SENSOR("", "Internal temperature", this->internal_temperature_sensor_);
  LOG_SENSOR("", "External temperature", this->external_temperature_sensor_);
}

}  // namespace bl0940
}  // namespace esphome
