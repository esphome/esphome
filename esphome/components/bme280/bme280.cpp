#include "bme280.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bme280 {

static const char *TAG = "bme280.sensor";

static const uint8_t BME280_REGISTER_DIG_T1 = 0x88;
static const uint8_t BME280_REGISTER_DIG_T2 = 0x8A;
static const uint8_t BME280_REGISTER_DIG_T3 = 0x8C;

static const uint8_t BME280_REGISTER_DIG_P1 = 0x8E;
static const uint8_t BME280_REGISTER_DIG_P2 = 0x90;
static const uint8_t BME280_REGISTER_DIG_P3 = 0x92;
static const uint8_t BME280_REGISTER_DIG_P4 = 0x94;
static const uint8_t BME280_REGISTER_DIG_P5 = 0x96;
static const uint8_t BME280_REGISTER_DIG_P6 = 0x98;
static const uint8_t BME280_REGISTER_DIG_P7 = 0x9A;
static const uint8_t BME280_REGISTER_DIG_P8 = 0x9C;
static const uint8_t BME280_REGISTER_DIG_P9 = 0x9E;

static const uint8_t BME280_REGISTER_DIG_H1 = 0xA1;
static const uint8_t BME280_REGISTER_DIG_H2 = 0xE1;
static const uint8_t BME280_REGISTER_DIG_H3 = 0xE3;
static const uint8_t BME280_REGISTER_DIG_H4 = 0xE4;
static const uint8_t BME280_REGISTER_DIG_H5 = 0xE5;
static const uint8_t BME280_REGISTER_DIG_H6 = 0xE7;

static const uint8_t BME280_REGISTER_CHIPID = 0xD0;

static const uint8_t BME280_REGISTER_CONTROLHUMID = 0xF2;
static const uint8_t BME280_REGISTER_STATUS = 0xF3;
static const uint8_t BME280_REGISTER_CONTROL = 0xF4;
static const uint8_t BME280_REGISTER_CONFIG = 0xF5;
static const uint8_t BME280_REGISTER_PRESSUREDATA = 0xF7;
static const uint8_t BME280_REGISTER_TEMPDATA = 0xFA;
static const uint8_t BME280_REGISTER_HUMIDDATA = 0xFD;

static const uint8_t BME280_MODE_FORCED = 0b01;

inline uint16_t combine_bytes(uint8_t msb, uint8_t lsb) { return ((msb & 0xFF) << 8) | (lsb & 0xFF); }

static const char *oversampling_to_str(BME280Oversampling oversampling) {
  switch (oversampling) {
    case BME280_OVERSAMPLING_NONE:
      return "None";
    case BME280_OVERSAMPLING_1X:
      return "1x";
    case BME280_OVERSAMPLING_2X:
      return "2x";
    case BME280_OVERSAMPLING_4X:
      return "4x";
    case BME280_OVERSAMPLING_8X:
      return "8x";
    case BME280_OVERSAMPLING_16X:
      return "16x";
    default:
      return "UNKNOWN";
  }
}

static const char *iir_filter_to_str(BME280IIRFilter filter) {
  switch (filter) {
    case BME280_IIR_FILTER_OFF:
      return "OFF";
    case BME280_IIR_FILTER_2X:
      return "2x";
    case BME280_IIR_FILTER_4X:
      return "4x";
    case BME280_IIR_FILTER_8X:
      return "8x";
    case BME280_IIR_FILTER_16X:
      return "16x";
    default:
      return "UNKNOWN";
  }
}

void BME280Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up BME280...");
  uint8_t chip_id = 0;
  if (!this->read_byte(BME280_REGISTER_CHIPID, &chip_id)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }
  if (chip_id != 0x60) {
    this->error_code_ = WRONG_CHIP_ID;
    this->mark_failed();
    return;
  }

  // Read calibration
  this->calibration_.t1 = read_u16_le_(BME280_REGISTER_DIG_T1);
  this->calibration_.t2 = read_s16_le_(BME280_REGISTER_DIG_T2);
  this->calibration_.t3 = read_s16_le_(BME280_REGISTER_DIG_T3);

  this->calibration_.p1 = read_u16_le_(BME280_REGISTER_DIG_P1);
  this->calibration_.p2 = read_s16_le_(BME280_REGISTER_DIG_P2);
  this->calibration_.p3 = read_s16_le_(BME280_REGISTER_DIG_P3);
  this->calibration_.p4 = read_s16_le_(BME280_REGISTER_DIG_P4);
  this->calibration_.p5 = read_s16_le_(BME280_REGISTER_DIG_P5);
  this->calibration_.p6 = read_s16_le_(BME280_REGISTER_DIG_P6);
  this->calibration_.p7 = read_s16_le_(BME280_REGISTER_DIG_P7);
  this->calibration_.p8 = read_s16_le_(BME280_REGISTER_DIG_P8);
  this->calibration_.p9 = read_s16_le_(BME280_REGISTER_DIG_P9);

  this->calibration_.h1 = read_u8_(BME280_REGISTER_DIG_H1);
  this->calibration_.h2 = read_s16_le_(BME280_REGISTER_DIG_H2);
  this->calibration_.h3 = read_u8_(BME280_REGISTER_DIG_H3);
  this->calibration_.h4 = read_u8_(BME280_REGISTER_DIG_H4) << 4 | (read_u8_(BME280_REGISTER_DIG_H4 + 1) & 0x0F);
  this->calibration_.h5 = read_u8_(BME280_REGISTER_DIG_H5 + 1) << 4 | (read_u8_(BME280_REGISTER_DIG_H5) >> 4);
  this->calibration_.h6 = read_u8_(BME280_REGISTER_DIG_H6);

  uint8_t humid_register = 0;
  if (!this->read_byte(BME280_REGISTER_CONTROLHUMID, &humid_register)) {
    this->mark_failed();
    return;
  }
  humid_register &= ~0b00000111;
  humid_register |= this->humidity_oversampling_ & 0b111;
  if (!this->write_byte(BME280_REGISTER_CONTROLHUMID, humid_register)) {
    this->mark_failed();
    return;
  }

  uint8_t config_register = 0;
  if (!this->read_byte(BME280_REGISTER_CONFIG, &config_register)) {
    this->mark_failed();
    return;
  }
  config_register &= ~0b11111100;
  config_register |= 0b000 << 5;  // 0.5 ms standby time
  config_register |= (this->iir_filter_ & 0b111) << 2;
  if (!this->write_byte(BME280_REGISTER_CONFIG, config_register)) {
    this->mark_failed();
    return;
  }
}
void BME280Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BME280:");
  LOG_I2C_DEVICE(this);
  switch (this->error_code_) {
    case COMMUNICATION_FAILED:
      ESP_LOGE(TAG, "Communication with BME280 failed!");
      break;
    case WRONG_CHIP_ID:
      ESP_LOGE(TAG, "BMP280 has wrong chip ID! Is it a BMP280?");
      break;
    case NONE:
    default:
      break;
  }
  ESP_LOGCONFIG(TAG, "  IIR Filter: %s", iir_filter_to_str(this->iir_filter_));
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  ESP_LOGCONFIG(TAG, "    Oversampling: %s", oversampling_to_str(this->temperature_oversampling_));
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
  ESP_LOGCONFIG(TAG, "    Oversampling: %s", oversampling_to_str(this->pressure_oversampling_));
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  ESP_LOGCONFIG(TAG, "    Oversampling: %s", oversampling_to_str(this->humidity_oversampling_));
}
float BME280Component::get_setup_priority() const { return setup_priority::DATA; }

inline uint8_t oversampling_to_time(BME280Oversampling over_sampling) { return (1 << uint8_t(over_sampling)) >> 1; }

void BME280Component::update() {
  // Enable sensor
  ESP_LOGV(TAG, "Sending conversion request...");
  uint8_t meas_register = 0;
  meas_register |= (this->temperature_oversampling_ & 0b111) << 5;
  meas_register |= (this->pressure_oversampling_ & 0b111) << 2;
  meas_register |= 0b01;  // Forced mode
  if (!this->write_byte(BME280_REGISTER_CONTROL, meas_register)) {
    this->status_set_warning();
    return;
  }

  float meas_time = 1.5;
  meas_time += 2.3f * oversampling_to_time(this->temperature_oversampling_);
  meas_time += 2.3f * oversampling_to_time(this->pressure_oversampling_) + 0.575f;
  meas_time += 2.3f * oversampling_to_time(this->humidity_oversampling_) + 0.575f;

  this->set_timeout("data", uint32_t(ceilf(meas_time)), [this]() {
    int32_t t_fine = 0;
    float temperature = this->read_temperature_(&t_fine);
    if (isnan(temperature)) {
      ESP_LOGW(TAG, "Invalid temperature, cannot read pressure & humidity values.");
      this->status_set_warning();
      return;
    }
    float pressure = this->read_pressure_(t_fine);
    float humidity = this->read_humidity_(t_fine);

    ESP_LOGD(TAG, "Got temperature=%.1f°C pressure=%.1fhPa humidity=%.1f%%", temperature, pressure, humidity);
    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(temperature);
    if (this->pressure_sensor_ != nullptr)
      this->pressure_sensor_->publish_state(pressure);
    if (this->humidity_sensor_ != nullptr)
      this->humidity_sensor_->publish_state(humidity);
    this->status_clear_warning();
  });
}
float BME280Component::read_temperature_(int32_t *t_fine) {
  uint8_t data[3];
  if (!this->read_bytes(BME280_REGISTER_TEMPDATA, data, 3))
    return NAN;
  int32_t adc = ((data[0] & 0xFF) << 16) | ((data[1] & 0xFF) << 8) | (data[2] & 0xFF);
  adc >>= 4;
  if (adc == 0x80000)
    // temperature was disabled
    return NAN;

  const int32_t t1 = this->calibration_.t1;
  const int32_t t2 = this->calibration_.t2;
  const int32_t t3 = this->calibration_.t3;

  int32_t var1 = (((adc >> 3) - (t1 << 1)) * t2) >> 11;
  int32_t var2 = (((((adc >> 4) - t1) * ((adc >> 4) - t1)) >> 12) * t3) >> 14;
  *t_fine = var1 + var2;

  float temperature = (*t_fine * 5 + 128) >> 8;
  return temperature / 100.0f;
}

float BME280Component::read_pressure_(int32_t t_fine) {
  uint8_t data[3];
  if (!this->read_bytes(BME280_REGISTER_PRESSUREDATA, data, 3))
    return NAN;
  int32_t adc = ((data[0] & 0xFF) << 16) | ((data[1] & 0xFF) << 8) | (data[2] & 0xFF);
  adc >>= 4;
  if (adc == 0x80000)
    // pressure was disabled
    return NAN;
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

float BME280Component::read_humidity_(int32_t t_fine) {
  uint16_t raw_adc;
  if (!this->read_byte_16(BME280_REGISTER_HUMIDDATA, &raw_adc) || raw_adc == 0x8000)
    return NAN;

  int32_t adc = raw_adc;

  const int32_t h1 = this->calibration_.h1;
  const int32_t h2 = this->calibration_.h2;
  const int32_t h3 = this->calibration_.h3;
  const int32_t h4 = this->calibration_.h4;
  const int32_t h5 = this->calibration_.h5;
  const int32_t h6 = this->calibration_.h6;

  int32_t v_x1_u32r = t_fine - 76800;

  v_x1_u32r = ((((adc << 14) - (h4 << 20) - (h5 * v_x1_u32r)) + 16384) >> 15) *
              (((((((v_x1_u32r * h6) >> 10) * (((v_x1_u32r * h3) >> 11) + 32768)) >> 10) + 2097152) * h2 + 8192) >> 14);

  v_x1_u32r = v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * h1) >> 4);

  v_x1_u32r = v_x1_u32r < 0 ? 0 : v_x1_u32r;
  v_x1_u32r = v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r;
  float h = v_x1_u32r >> 12;

  return h / 1024.0f;
}
void BME280Component::set_temperature_oversampling(BME280Oversampling temperature_over_sampling) {
  this->temperature_oversampling_ = temperature_over_sampling;
}
void BME280Component::set_pressure_oversampling(BME280Oversampling pressure_over_sampling) {
  this->pressure_oversampling_ = pressure_over_sampling;
}
void BME280Component::set_humidity_oversampling(BME280Oversampling humidity_over_sampling) {
  this->humidity_oversampling_ = humidity_over_sampling;
}
void BME280Component::set_iir_filter(BME280IIRFilter iir_filter) { this->iir_filter_ = iir_filter; }
uint8_t BME280Component::read_u8_(uint8_t a_register) {
  uint8_t data = 0;
  this->read_byte(a_register, &data);
  return data;
}
uint16_t BME280Component::read_u16_le_(uint8_t a_register) {
  uint16_t data = 0;
  this->read_byte_16(a_register, &data);
  return (data >> 8) | (data << 8);
}
int16_t BME280Component::read_s16_le_(uint8_t a_register) { return this->read_u16_le_(a_register); }

}  // namespace bme280
}  // namespace esphome
