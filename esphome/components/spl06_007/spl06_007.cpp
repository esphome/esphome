#include "spl06_007.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace spl06_007 {

static const char *const TAG = "spl06_007.sensor";

// coefficient registers
static const uint8_t SPL06_007_REGISTER_C0 = 0x10;
static const uint8_t SPL06_007_REGISTER_C0_C1 = 0x11;
static const uint8_t SPL06_007_REGISTER_C1 = 0x12;
static const uint8_t SPL06_007_REGISTER_C00_13 = 0x13;
static const uint8_t SPL06_007_REGISTER_C00_14 = 0x14;
static const uint8_t SPL06_007_REGISTER_C00_C10 = 0x15;
static const uint8_t SPL06_007_REGISTER_C10_16 = 0x16;
static const uint8_t SPL06_007_REGISTER_C10_17 = 0x17;
static const uint8_t SPL06_007_REGISTER_C01_18 = 0x18;
static const uint8_t SPL06_007_REGISTER_C01_19 = 0x19;
static const uint8_t SPL06_007_REGISTER_C11_1A = 0x1A;
static const uint8_t SPL06_007_REGISTER_C11_1B = 0x1B;
static const uint8_t SPL06_007_REGISTER_C20_1C = 0x1C;
static const uint8_t SPL06_007_REGISTER_C20_1D = 0x1D;
static const uint8_t SPL06_007_REGISTER_C21_1E = 0x1E;
static const uint8_t SPL06_007_REGISTER_C21_1F = 0x1F;
static const uint8_t SPL06_007_REGISTER_C30_20 = 0x20;
static const uint8_t SPL06_007_REGISTER_C30_21 = 0x21;

// setup related
static const uint8_t SPL06_007_REGISTER_RESET = 0x0C;
static const uint8_t SPL06_007_SOFT_RESET = 0b1001;
static const uint8_t SPL06_007_REGISTER_CHIP_ID = 0x0D;
static const uint8_t SPL06_007_REGISTER_STATUS = 0x08;
static const uint8_t SPL06_007_STATUS_READY = 0b11;
static const uint8_t SPL06_007_REGISTER_PRS_CFG = 0x06;
static const uint8_t SPL06_007_REGISTER_TMP_CFG = 0x07;

// measurements
static const uint8_t SPL06_007_REGISTER_MEASUREMENTS = 0x00;

static const char *oversampling_to_str(SPL06_007PressureOversampling oversampling) {
  switch (oversampling) {
    case SPL06_007_PRESSURE_OVERSAMPLING_1X:
      return "1x";
    case SPL06_007_PRESSURE_OVERSAMPLING_2X:
      return "2x";
    case SPL06_007_PRESSURE_OVERSAMPLING_4X:
      return "4x";
    case SPL06_007_PRESSURE_OVERSAMPLING_8X:
      return "8x";
    case SPL06_007_PRESSURE_OVERSAMPLING_16X:
      return "16x";
    case SPL06_007_PRESSURE_OVERSAMPLING_32X:
      return "32x";
    case SPL06_007_PRESSURE_OVERSAMPLING_64X:
      return "64x";
    case SPL06_007_PRESSURE_OVERSAMPLING_128X:
      return "128x";
    default:
      return "UNKNOWN";
  }
}

static const char *oversampling_to_str(SPL06_007TemperatureOversampling oversampling) {
  switch (oversampling) {
    case SPL06_007_TEMPERATURE_OVERSAMPLING_1X:
      return "1x";
    case SPL06_007_TEMPERATURE_OVERSAMPLING_2X:
      return "2x";
    case SPL06_007_TEMPERATURE_OVERSAMPLING_4X:
      return "4x";
    case SPL06_007_TEMPERATURE_OVERSAMPLING_8X:
      return "8x";
    case SPL06_007_TEMPERATURE_OVERSAMPLING_16X:
      return "16x";
    case SPL06_007_TEMPERATURE_OVERSAMPLING_32X:
      return "32x";
    case SPL06_007_TEMPERATURE_OVERSAMPLING_64X:
      return "64x";
    case SPL06_007_TEMPERATURE_OVERSAMPLING_128X:
      return "128x";
    default:
      return "UNKNOWN";
  }
}

void SPL06_007Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SPL06-007...");
  uint8_t chip_id = 0;

  // Mark as not failed before initializing. Some devices will turn off sensors to save on batteries
  // and when they come back on, the COMPONENT_STATE_FAILED bit must be unset on the component.
  if ((this->component_state_ & COMPONENT_STATE_MASK) == COMPONENT_STATE_FAILED) {
    this->component_state_ &= ~COMPONENT_STATE_MASK;
    this->component_state_ |= COMPONENT_STATE_CONSTRUCTION;
  }

  if (!this->read_byte(SPL06_007_REGISTER_CHIP_ID, &chip_id)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }
  if (chip_id != 0x10) {
    ESP_LOGE(TAG, "Device Chip ID should be 0x10 but is: %hhu", chip_id);
    this->error_code_ = WRONG_CHIP_ID;
    this->mark_failed();
    return;
  }

  // Send a soft reset.
  if (!this->write_byte(SPL06_007_REGISTER_RESET, SPL06_007_SOFT_RESET)) {
    this->mark_failed();
    return;
  }
  // Wait until the NVM data has finished loading.
  uint8_t status;
  uint8_t retry = 10;
  do {
    ESP_LOGI(TAG, "retry is %d", retry);
    delay(5);  // this seems to be the bare minimum value
    if (!this->read_byte(SPL06_007_REGISTER_STATUS, &status)) {
      ESP_LOGW(TAG, "Error reading status register.");
      this->mark_failed();
      return;
    }
  } while (((status >> 6) ^ SPL06_007_STATUS_READY) && (--retry));

  if ((status >> 6) ^ SPL06_007_STATUS_READY) {
    ESP_LOGW(TAG, "Timeout loading NVM.");
    this->mark_failed();
    return;
  }

  // Read calibration temperature calibration
  int16_t c0;

  uint8_t c0_msb = read_u8_(SPL06_007_REGISTER_C0);
  uint8_t c0_lsb_c1_msb = read_u8_(SPL06_007_REGISTER_C0_C1);
  uint8_t c1_lsb = read_u8_(SPL06_007_REGISTER_C1);

  c0 = ((c0_msb << 4) | (c0_lsb_c1_msb >> 4));

  if (c0 & (1 << 11))  // Check for 2's complement negative number
    c0 = c0 | 0XF000;  // Set left bits to one for 2's complement conversion of negative number
  ESP_LOGV(TAG, "C0:" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(c0));
  this->calibration_.c0 = c0;

  // c1
  int16_t c1;
  c1 = (((c0_lsb_c1_msb & 0XF) << 8) | c1_lsb);

  if (c1 & (1 << 11)) {  // Check for 2's complement negative number
    c1 = c1 | 0XF000;    // Set left bits to one for 2's complement conversion of negative number
  }
  ESP_LOGV(TAG, "C1:" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(c1));
  this->calibration_.c1 = c1;

  // Read pressure calibration
  // c00
  uint8_t c00_msb = read_u8_(SPL06_007_REGISTER_C00_13);
  uint8_t c00_lsb = read_u8_(SPL06_007_REGISTER_C00_14);
  uint8_t c00_c10 = read_u8_(SPL06_007_REGISTER_C00_C10);
  uint8_t c00_xlsb = c00_c10 >> 4;

  int32_t c00 = (((c00_msb << 8) | c00_lsb) << 4) | c00_xlsb;
  if (c00 >> 19) {
    c00 = c00 | 0XFFF00000;
  }  // Set left bits to one for 2's complement conversion of negative number

  this->calibration_.c00 = c00;
  ESP_LOGD(TAG, "C00: %d", this->calibration_.c00);

  // c10
  int32_t c10;

  uint8_t c10_lsb = read_u8_(SPL06_007_REGISTER_C10_16);
  uint8_t c10_xlsb = read_u8_(SPL06_007_REGISTER_C10_17);

  c10 = ((((c00_c10 & 0b00001111) << 8) | c10_lsb) << 8) | c10_xlsb;

  if (c10 >> 19)
    c10 = c10 | 0XFFF00000;  // Set left bits to one for 2's complement conversion of negative number
  this->calibration_.c10 = c10;
  ESP_LOGD(TAG, "C10: %d", this->calibration_.c10);

  // c01
  uint8_t c01_msb = read_u8_(SPL06_007_REGISTER_C01_18);
  uint8_t c01_lsb = read_u8_(SPL06_007_REGISTER_C01_19);
  this->calibration_.c01 = (c01_msb << 8) | c01_lsb;
  ESP_LOGD(TAG, "C01: %d", this->calibration_.c01);

  // c11
  uint8_t c11_msb = read_u8_(SPL06_007_REGISTER_C11_1A);
  uint8_t c11_lsb = read_u8_(SPL06_007_REGISTER_C11_1B);
  this->calibration_.c11 = (c11_msb << 8) | c11_lsb;
  ESP_LOGD(TAG, "C11: %d", this->calibration_.c11);

  // c20
  uint8_t c20_msb = read_u8_(SPL06_007_REGISTER_C20_1C);
  uint8_t c20_lsb = read_u8_(SPL06_007_REGISTER_C20_1D);
  this->calibration_.c20 = (c20_msb << 8) | c20_lsb;
  ESP_LOGD(TAG, "C20: %d", this->calibration_.c20);

  // c21
  uint8_t c21_msb = read_u8_(SPL06_007_REGISTER_C21_1E);
  uint8_t c21_lsb = read_u8_(SPL06_007_REGISTER_C21_1F);
  this->calibration_.c21 = (c21_msb << 8) | c21_lsb;
  ESP_LOGD(TAG, "C21: %d", this->calibration_.c21);

  // c30
  uint8_t c30_msb = read_u8_(SPL06_007_REGISTER_C30_20);
  uint8_t c30_lsb = read_u8_(SPL06_007_REGISTER_C30_21);
  this->calibration_.c30 = (c30_msb << 8) | c30_lsb;
  ESP_LOGD(TAG, "C30: %d", this->calibration_.c30);

  // set temperature sampling
  uint8_t tmp_cfg = 0;

  tmp_cfg |= 0b1 << 7;      // external sensor (always)
  tmp_cfg |= (0b000) << 4;  // 1 measurement per second
  tmp_cfg |= (0b0) << 3;    // reserved
  tmp_cfg |= (this->temperature_oversampling_);

  ESP_LOGD(TAG, "TMP_CFG:" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(tmp_cfg));

  this->write_byte(SPL06_007_REGISTER_TMP_CFG, tmp_cfg);

  // set pressure sampling
  uint8_t prs_cfg = 0;

  prs_cfg |= 0b0 << 7;      // reserved
  prs_cfg |= (0b000) << 4;  // 1 measurement per second
  prs_cfg |= (this->pressure_oversampling_);

  ESP_LOGD(TAG, "PRS_CFG:" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(prs_cfg));

  this->write_byte(SPL06_007_REGISTER_PRS_CFG, prs_cfg);
}
void SPL06_007Component::dump_config() {
  ESP_LOGCONFIG(TAG, "SPL06-007:");
  LOG_I2C_DEVICE(this);
  switch (this->error_code_) {
    case COMMUNICATION_FAILED:
      ESP_LOGW(TAG, "Communication with SPL06-007 failed!");
      break;
    case WRONG_CHIP_ID:
      ESP_LOGW(TAG, "SPL06-007 has wrong chip ID! Is it a SPL06-007?");
      break;
    case NONE:
    default:
      break;
  }
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  ESP_LOGCONFIG(TAG, "    Oversampling: %s", oversampling_to_str(this->temperature_oversampling_));
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
  ESP_LOGCONFIG(TAG, "    Oversampling: %s", oversampling_to_str(this->pressure_oversampling_));
}
float SPL06_007Component::get_setup_priority() const { return setup_priority::DATA; }

template<class T> uint8_t oversampling_to_time(T over_sampling) { return (1 << uint8_t(over_sampling)); }

template<class T> double_t oversampling_to_factor(T over_sampling) {
  uint8_t idx = 1 << uint8_t(over_sampling);
  switch (idx) {
    case 1:
      return 524288.0;
    case 2:
      return 1572864.0;
    case 4:
      return 3670016.0;
    case 8:
      return 7864320.0;
    case 16:
      return 253952.0;
    case 32:
      return 516096.0;
    case 64:
      return 1040384.0;
    case 128:
      return 2088960.0;
  }
  return NAN;
}

void SPL06_007Component::update() {
  // Enable sensor
  ESP_LOGI(TAG, "Configured Temp Oversampling is: %hhu", oversampling_to_time(this->temperature_oversampling_));
  ESP_LOGI(TAG, "Configured Pressure Oversampling: %hhu", oversampling_to_time(this->pressure_oversampling_));

  ESP_LOGI(TAG, "Sending conversion request...");
  if (!this->write_byte(SPL06_007_REGISTER_STATUS, 0b111)) {
    this->status_set_warning();
    return;
  }

  this->set_timeout("data", 10, [this]() {
    uint8_t data[6];
    //    uint8_t press_data[6];
    if (!this->read_bytes(SPL06_007_REGISTER_MEASUREMENTS, data, 6)) {
      ESP_LOGE(TAG, "Error reading temperature measurement registers.");
      this->status_set_warning();
      return;
    }

    float temperature = this->read_temperature_(data);
    if (std::isnan(temperature)) {
      ESP_LOGW(TAG, "Invalid temperature, cannot read pressure values.");
      this->status_set_warning();
      return;
    }

    float pressure = this->read_pressure_(data);

    ESP_LOGV(TAG, "Got temperature=%.1f°C pressure=%.1fhPa", temperature, pressure);
    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(temperature);
    if (this->pressure_sensor_ != nullptr)
      this->pressure_sensor_->publish_state(pressure);
    this->status_clear_warning();
  });
}

int32_t get_temp_raw_(const uint8_t *data) {
  int32_t tRaw;
  tRaw = (data[3] << 8) | data[4];
  tRaw = (tRaw << 8) | data[5];

  if (tRaw & (1 << 23))
    tRaw = tRaw | 0XFF000000;  // Set left bits to one for 2's complement conversion of negative number

  return tRaw;
}

double get_temp_raw_sc(const int32_t tRaw, const double temp_compensation_factor) {
  return (double(tRaw) / temp_compensation_factor);
}

float SPL06_007Component::read_temperature_(const uint8_t *data) {
  int32_t tRaw = get_temp_raw_(data);
  ESP_LOGD(TAG, "Raw Temperature: %f, comp: %f", double(tRaw), double(this->temp_compensation_factor_));
  double tRawSc = get_temp_raw_sc(double(tRaw), this->temp_compensation_factor_);
  ESP_LOGD(TAG, "Raw Temperature SC: %f", tRawSc);

  float temp = double(this->calibration_.c0) * 0.5f + double(this->calibration_.c1) * tRawSc;
  ESP_LOGD(TAG, "Temperature: %f", temp);
  return temp;
}

float SPL06_007Component::read_pressure_(const uint8_t *data) {
  // start by recalculating tRawSc
  int32_t tRaw = get_temp_raw_(data);
  double tRawSc = get_temp_raw_sc(tRaw, this->temp_compensation_factor_);

  // then read pressure data
  int32_t pRaw;
  pRaw = (data[0] << 8) | data[1];
  pRaw = (pRaw << 8) | data[2];

  if (pRaw & (1 << 23))
    pRaw = pRaw | 0XFF000000;  // Set left bits to one for 2's complement conversion of negative number

  double pRawSc = double(pRaw) / this->pressure_compensation_factor_;
  ESP_LOGD(TAG, "Raw Temperature SC: %f, Pressure Compensation Factor: %f, Raw Pressure: %d, Compensated Pressure: %f",
           double(tRawSc), this->pressure_compensation_factor_, pRaw, pRawSc);

  double pcomp = double(this->calibration_.c00) +
                 pRawSc * (double(this->calibration_.c10) +
                           pRawSc * (double(this->calibration_.c20) + pRawSc * double(this->calibration_.c30))) +
                 tRawSc * double(this->calibration_.c01) +
                 tRawSc * pRawSc * (double(this->calibration_.c11) + pRawSc * double(this->calibration_.c21));
  ESP_LOGD(TAG, "Pressure: %f", pcomp / 100);
  return pcomp / 100;
}

void SPL06_007Component::set_temperature_oversampling(SPL06_007TemperatureOversampling temperature_over_sampling) {
  this->temperature_oversampling_ = temperature_over_sampling;
  this->temp_compensation_factor_ = oversampling_to_factor(temperature_over_sampling);
}
void SPL06_007Component::set_pressure_oversampling(SPL06_007PressureOversampling pressure_over_sampling) {
  this->pressure_oversampling_ = pressure_over_sampling;
  this->pressure_compensation_factor_ = oversampling_to_factor(pressure_over_sampling);
}

uint8_t SPL06_007Component::read_u8_(uint8_t a_register) {
  uint8_t data = 0;
  this->read_byte(a_register, &data);
  return data;
}

}  // namespace spl06_007
}  // namespace esphome
