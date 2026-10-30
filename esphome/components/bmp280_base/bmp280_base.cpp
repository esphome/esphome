#include "bmp280_base.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::bmp280_base {

ESPHOME_LOG_TAG(TAG, "bmp280.sensor");

static constexpr uint32_t DATA_TIMEOUT_ID = 0;

static const uint8_t BMP280_REGISTER_STATUS = 0xF3;
static const uint8_t BMP280_REGISTER_CONTROL = 0xF4;
static const uint8_t BMP280_REGISTER_CONFIG = 0xF5;
static const uint8_t BMP280_REGISTER_PRESSUREDATA = 0xF7;
static const uint8_t BMP280_REGISTER_TEMPDATA = 0xFA;
static const uint8_t BMP280_REGISTER_RESET = 0xE0;

static const uint8_t BMP280_MODE_FORCED = 0b01;
static const uint8_t BMP280_SOFT_RESET = 0xB6;
static const uint8_t BMP280_STATUS_IM_UPDATE = 0b01;

inline uint16_t combine_bytes(uint8_t msb, uint8_t lsb) { return ((msb & 0xFF) << 8) | (lsb & 0xFF); }

// Indexed by BMP280Oversampling
PROGMEM_STRING_TABLE(OversamplingStrings, "None", "1x", "2x", "4x", "8x", "16x", "UNKNOWN");

[[maybe_unused]] static const LogString *oversampling_to_str(BMP280Oversampling value) {
  return OversamplingStrings::get_log_str(static_cast<uint8_t>(value), OversamplingStrings::LAST_INDEX);
}

// Indexed by BMP280IIRFilter
PROGMEM_STRING_TABLE(IIRFilterStrings, "OFF", "2x", "4x", "8x", "16x", "UNKNOWN");

[[maybe_unused]] static const LogString *iir_filter_to_str(BMP280IIRFilter value) {
  return IIRFilterStrings::get_log_str(static_cast<uint8_t>(value), IIRFilterStrings::LAST_INDEX);
}

void BMP280Component::setup() {
  uint8_t chip_id = 0;

  // Read the chip id twice, to work around a bug where the first read is 0.
  // https://community.st.com/t5/stm32-mcus-products/issue-with-reading-bmp280-chip-id-using-spi/td-p/691855
  if (!this->bmp_read_byte(0xD0, &chip_id)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  if (!this->bmp_read_byte(0xD0, &chip_id)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  if (chip_id != 0x58) {
    this->mark_failed(LOG_STR("Wrong chip ID or no response"));
    return;
  }

  // Send a soft reset.
  if (!this->bmp_write_byte(BMP280_REGISTER_RESET, BMP280_SOFT_RESET)) {
    this->mark_failed(LOG_STR("Reset failed"));
    return;
  }
  // Wait until the NVM data has finished loading.
  uint8_t status;
  uint8_t retry = 5;
  do {
    delay(2);
    if (!this->bmp_read_byte(BMP280_REGISTER_STATUS, &status)) {
      this->mark_failed(LOG_STR("Error reading status register"));
      return;
    }
  } while ((status & BMP280_STATUS_IM_UPDATE) && (--retry));
  if (status & BMP280_STATUS_IM_UPDATE) {
    this->mark_failed(LOG_STR("Timeout loading NVM"));
    return;
  }

  // Read calibration
  this->calibration_.t1 = this->read_u16_le_(0x88);
  this->calibration_.t2 = this->read_s16_le_(0x8A);
  this->calibration_.t3 = this->read_s16_le_(0x8C);

  this->calibration_.p1 = this->read_u16_le_(0x8E);
  this->calibration_.p2 = this->read_s16_le_(0x90);
  this->calibration_.p3 = this->read_s16_le_(0x92);
  this->calibration_.p4 = this->read_s16_le_(0x94);
  this->calibration_.p5 = this->read_s16_le_(0x96);
  this->calibration_.p6 = this->read_s16_le_(0x98);
  this->calibration_.p7 = this->read_s16_le_(0x9A);
  this->calibration_.p8 = this->read_s16_le_(0x9C);
  this->calibration_.p9 = this->read_s16_le_(0x9E);

  uint8_t config_register = 0;
  if (!this->bmp_read_byte(BMP280_REGISTER_CONFIG, &config_register)) {
    this->mark_failed(LOG_STR("Read config"));
    return;
  }
  config_register &= ~0b11111100;
  config_register |= 0b000 << 5;  // 0.5 ms standby time
  config_register |= (this->iir_filter_ & 0b111) << 2;
  if (!this->bmp_write_byte(BMP280_REGISTER_CONFIG, config_register)) {
    this->mark_failed(LOG_STR("Write config"));
    return;
  }
}
void BMP280Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BMP280:");
  ESP_LOGCONFIG(TAG, "  IIR Filter: %s", LOG_STR_ARG(iir_filter_to_str(this->iir_filter_)));
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  ESP_LOGCONFIG(TAG, "    Oversampling: %s", LOG_STR_ARG(oversampling_to_str(this->temperature_oversampling_)));
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
  ESP_LOGCONFIG(TAG, "    Oversampling: %s", LOG_STR_ARG(oversampling_to_str(this->pressure_oversampling_)));
}

inline uint8_t oversampling_to_time(BMP280Oversampling over_sampling) { return (1 << uint8_t(over_sampling)) >> 1; }

void BMP280Component::update() {
  // Enable sensor
  ESP_LOGV(TAG, "Sending conversion request");
  uint8_t meas_value = 0;
  meas_value |= (this->temperature_oversampling_ & 0b111) << 5;
  meas_value |= (this->pressure_oversampling_ & 0b111) << 2;
  meas_value |= 0b01;  // Forced mode
  if (!this->bmp_write_byte(BMP280_REGISTER_CONTROL, meas_value)) {
    this->status_set_warning();
    return;
  }

  float meas_time = 1;
  meas_time += 2.3f * oversampling_to_time(this->temperature_oversampling_);
  meas_time += 2.3f * oversampling_to_time(this->pressure_oversampling_) + 0.575f;

  this->set_timeout(DATA_TIMEOUT_ID, uint32_t(ceilf(meas_time)), [this]() {
    int32_t t_fine = 0;
    float temperature = this->read_temperature_(&t_fine);
    if (std::isnan(temperature)) {
      ESP_LOGW(TAG, "Invalid temperature");
      this->status_set_warning();
      return;
    }
    float pressure = this->read_pressure_(t_fine);

    ESP_LOGV(TAG, "Temperature=%.1f°C Pressure=%.1fhPa", temperature, pressure);
    PUBLISH_STATE(this->temperature_sensor_, temperature);
    PUBLISH_STATE(this->pressure_sensor_, pressure);
    this->status_clear_warning();
  });
}

float BMP280Component::read_temperature_(int32_t *t_fine) {
  uint8_t data[3]{};
  if (!this->bmp_read_bytes(BMP280_REGISTER_TEMPDATA, data, 3))
    return NAN;
  ESP_LOGV(TAG, "Read temperature data, raw: %02X %02X %02X", data[0], data[1], data[2]);
  int32_t adc = encode_uint24(data[0], data[1], data[2]);
  adc >>= 4;
  if (adc == 0x80000) {
    // temperature was disabled
    return NAN;
  }

  const int32_t t1 = this->calibration_.t1;
  const int32_t t2 = this->calibration_.t2;
  const int32_t t3 = this->calibration_.t3;

  int32_t var1 = (((adc >> 3) - (t1 << 1)) * t2) >> 11;
  int32_t var2 = (((((adc >> 4) - t1) * ((adc >> 4) - t1)) >> 12) * t3) >> 14;
  *t_fine = var1 + var2;

  float temperature = (*t_fine * 5 + 128);
  return temperature / 25600.0f;
}

float BMP280Component::read_pressure_(int32_t t_fine) {
  uint8_t data[3];
  if (!this->bmp_read_bytes(BMP280_REGISTER_PRESSUREDATA, data, 3))
    return NAN;
  int32_t adc = encode_uint24(data[0], data[1], data[2]);
  adc >>= 4;
  if (adc == 0x80000) {
    // pressure was disabled
    return NAN;
  }
  const int64_t p1 = this->calibration_.p1;
  const int64_t p2 = this->calibration_.p2;
  const int64_t p3 = this->calibration_.p3;
  const int64_t p4 = this->calibration_.p4;
  const int64_t p5 = this->calibration_.p5;
  const int64_t p6 = this->calibration_.p6;
  const int64_t p7 = this->calibration_.p7;
  const int64_t p8 = this->calibration_.p8;
  const int64_t p9 = this->calibration_.p9;

  int64_t var1, var2, p;
  var1 = int64_t(t_fine) - 128000;
  var2 = var1 * var1 * p6;
  var2 = var2 + ((var1 * p5) << 17);
  var2 = var2 + (p4 << 35);
  var1 = ((var1 * var1 * p3) >> 8) + ((var1 * p2) << 12);
  var1 = ((int64_t(1) << 47) + var1) * p1 >> 33;

  if (var1 == 0)
    return NAN;

  p = 1048576 - adc;
  p = (((p << 31) - var2) * 3125) / var1;
  var1 = (p9 * (p >> 13) * (p >> 13)) >> 25;
  var2 = (p8 * p) >> 19;

  p = ((p + var1 + var2) >> 8) + (p7 << 4);
  return (p / 256.0f) / 100.0f;
}
void BMP280Component::set_temperature_oversampling(BMP280Oversampling temperature_over_sampling) {
  this->temperature_oversampling_ = temperature_over_sampling;
}
void BMP280Component::set_pressure_oversampling(BMP280Oversampling pressure_over_sampling) {
  this->pressure_oversampling_ = pressure_over_sampling;
}
uint8_t BMP280Component::read_u8_(uint8_t a_register) {
  uint8_t data = 0;
  this->bmp_read_byte(a_register, &data);
  return data;
}
uint16_t BMP280Component::read_u16_le_(uint8_t a_register) {
  uint16_t data = 0;
  this->bmp_read_byte_16_le(a_register, &data);
  return data;
}
int16_t BMP280Component::read_s16_le_(uint8_t a_register) { return this->read_u16_le_(a_register); }

}  // namespace esphome::bmp280_base
