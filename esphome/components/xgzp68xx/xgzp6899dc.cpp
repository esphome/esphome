#include "xgzp6899dc.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cstddef>

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

// 2^exponent built from its bits, without pulling in libm; valid for exponents -126 to 127
static float power_of_two(int exponent) { return bit_cast<float>(static_cast<uint32_t>(127 + exponent) << 23); }

XGZP6899DCComponent::XGZP6899DCComponent(float pressure_span) : pressure_scale_(pressure_span / PRESSURE_FULL_SCALE) {}

void XGZP6899DCComponent::setup() {
  uint8_t chip_id;
  uint8_t cal_offset;
  uint8_t cal_shift;
  if (!this->read_byte(ID_REGISTER, &chip_id) || !this->read_byte(CAL_OFFSET_REGISTER, &cal_offset) ||
      !this->read_byte(CAL_SHIFT_REGISTER, &cal_shift)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  // The ID register holds the I2C address of the part, so a mismatch most likely means a different sensor
  if (chip_id != this->get_i2c_address()) {
    ESP_LOGW(TAG, "ID register reads 0x%02X, expected 0x%02X; check the configured variant", chip_id,
             this->get_i2c_address());
  }

  // Temperature = (raw - offset) / 2^shift + 25. Offset bits 6:0 are a power-of-two exponent and bit 7 is the
  // sign, e.g. 0x8D = -8192; the shift is a tenth of its register
  const float offset = power_of_two(cal_offset & 0x7F) * ((cal_offset & 0x80) ? -1.0f : 1.0f);
  this->temperature_scale_ = power_of_two(-(cal_shift / 10));
  this->temperature_bias_ = TEMPERATURE_REFERENCE - offset * this->temperature_scale_;
  ESP_LOGD(TAG, "Cal_coff 0x%02X 0x%02X (offset %.0f, shift %u)", cal_offset, cal_shift, offset, cal_shift / 10);

  if (this->pressure_oversampling_ != UINT8_MAX) {
    if (!this->update_register(CFG_OSR_REGISTER, CFG_OSR_P_MASK | CFG_OSR_MODE_MASK,
                               (this->pressure_oversampling_ << 2) | CFG_OSR_MODE_NORMAL)) {
      this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
      return;
    }
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

  PUBLISH_STATE(this->pressure_sensor_, static_cast<float>(sign_extend<24>(pressure_raw)) * this->pressure_scale_);
  PUBLISH_STATE(this->temperature_sensor_, temperature_raw * this->temperature_scale_ + this->temperature_bias_);
}

void XGZP6899DCComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "XGZP6899DC:\n"
                "  Pressure span: %.0f Pa",
                this->pressure_scale_ * PRESSURE_FULL_SCALE);
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
