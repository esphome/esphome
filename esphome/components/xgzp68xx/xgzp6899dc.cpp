#include "xgzp6899dc.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cstddef>
#include <cmath>

namespace esphome::xgzp68xx {

ESPHOME_LOG_TAG(TAG, "xgzp6899dc.sensor");

static constexpr uint32_t MEASUREMENT_TIMEOUT_ID = 0;

static constexpr uint8_t ID_REGISTER = 0x00;
static constexpr uint8_t CHIP_CONTROL_REGISTER = 0x01;
static constexpr uint8_t CFG_OSR_REGISTER = 0x02;
static constexpr uint8_t DATA_REGISTER = 0x04;  // 3 pressure bytes then 2 temperature bytes, MSB first
static constexpr uint8_t CAL_OFFSET_REGISTER = 0x20;
static constexpr uint8_t CAL_SHIFT_REGISTER = 0x21;

static constexpr uint8_t START_MEASUREMENT = 0x01;
static constexpr uint8_t CFG_OSR_P_MASK = 0x1C;
static constexpr uint8_t CFG_OSR_MODE_MASK = 0x03;
static constexpr uint8_t CFG_OSR_MODE_NORMAL = 0x01;
static constexpr uint32_t MEASUREMENT_TIME_MS = 20;
// Pressure (Pa) = signed 24-bit raw / 2^21 * (PMAX - PMIN)
static constexpr float PRESSURE_FULL_SCALE = 2097152.0f;
static constexpr float TEMPERATURE_REFERENCE = 25.0f;

void XGZP6899DCComponent::setup() {
  if (!this->read_byte(ID_REGISTER, &this->chip_id_) || !this->read_byte(CAL_OFFSET_REGISTER, &this->cal_offset_) ||
      !this->read_byte(CAL_SHIFT_REGISTER, &this->cal_shift_)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  // The ID register holds the I2C address of the part, so a mismatch most likely means a different sensor
  if (this->chip_id_ != this->get_i2c_address()) {
    ESP_LOGW(TAG, "ID register reads 0x%02X, expected 0x%02X; check the configured variant", this->chip_id_,
             this->get_i2c_address());
  }

  // Bits 6:0 are a power-of-two exponent and bit 7 is the sign, e.g. 0x8D = -8192
  this->temperature_offset_ = std::ldexp((this->cal_offset_ & 0x80) ? -1.0f : 1.0f, this->cal_offset_ & 0x7F);
  this->temperature_shift_ = this->cal_shift_ / 10;

  if (this->pressure_oversampling_ != UINT8_MAX) {
    uint8_t config;
    if (!this->read_byte(CFG_OSR_REGISTER, &config)) {
      this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
      return;
    }
    uint8_t new_config = (config & ~(CFG_OSR_P_MASK | CFG_OSR_MODE_MASK)) |
                         ((this->pressure_oversampling_ << 2) & CFG_OSR_P_MASK) | CFG_OSR_MODE_NORMAL;
    if (!this->write_byte(CFG_OSR_REGISTER, new_config)) {
      this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
      return;
    }
    ESP_LOGD(TAG, "CFG_OSR 0x%02X -> 0x%02X", config, new_config);
  }
}

void XGZP6899DCComponent::update() {
  if (!this->status_comm_result(this->write_byte(CHIP_CONTROL_REGISTER, START_MEASUREMENT)))
    return;
  this->set_timeout(MEASUREMENT_TIMEOUT_ID, MEASUREMENT_TIME_MS, [this]() { this->read_data_(); });
}

void XGZP6899DCComponent::read_data_() {
  // The datasheet only documents single-register reads, so read each byte separately
  uint8_t data[5];
  for (size_t i = 0; i < sizeof(data); i++) {
    if (!this->status_comm_result(this->read_byte(DATA_REGISTER + i, &data[i])))
      return;
  }
  const uint32_t pressure_raw = encode_uint24(data[0], data[1], data[2]);
  const int16_t temperature_raw = static_cast<int16_t>(encode_uint16(data[3], data[4]));
  ESP_LOGV(TAG, "Got raw pressure=%" PRIu32 ", raw temperature=%d", pressure_raw, temperature_raw);

  if (this->pressure_sensor_ != nullptr) {
    const float pressure =
        static_cast<float>(sign_extend<24>(pressure_raw)) / PRESSURE_FULL_SCALE * this->pressure_span_;
    this->pressure_sensor_->publish_state(pressure);
  }
  if (this->temperature_sensor_ != nullptr) {
    const float temperature =
        std::ldexp(temperature_raw - this->temperature_offset_, -this->temperature_shift_) + TEMPERATURE_REFERENCE;
    this->temperature_sensor_->publish_state(temperature);
  }
}

void XGZP6899DCComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "XGZP6899DC:\n"
                "  Chip ID: 0x%02X\n"
                "  Pressure span: %.0f Pa\n"
                "  Cal_coff: 0x%02X 0x%02X (offset %.0f, shift %u)",
                this->chip_id_, this->pressure_span_, this->cal_offset_, this->cal_shift_, this->temperature_offset_,
                this->temperature_shift_);
  LOG_SENSOR("  ", "Temperature: ", this->temperature_sensor_);
  LOG_SENSOR("  ", "Pressure: ", this->pressure_sensor_);
  if (this->pressure_sensor_ != nullptr) {
    if (this->pressure_oversampling_ == UINT8_MAX) {
      ESP_LOGCONFIG(TAG, "    Oversampling: factory setting");
    } else {
      ESP_LOGCONFIG(TAG, "    Oversampling: %" PRIu32 "x", UINT32_C(256) << this->pressure_oversampling_);
    }
  }
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::xgzp68xx
