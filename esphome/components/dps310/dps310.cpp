#include "dps310.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace dps310 {

static const char *const TAG = "dps310";

void DPS310Component::setup() {
  uint8_t coef_data_raw[DPS310_NUM_COEF_REGS];
  auto timer = DPS310_INIT_TIMEOUT;
  uint8_t reg = 0;

  ESP_LOGCONFIG(TAG, "Setting up DPS310...");
  // first, reset the sensor
  if (!this->write_byte(DPS310_REG_RESET, DPS310_CMD_RESET)) {
    this->mark_failed();
    return;
  }
  delay(10);
  // wait for the sensor and its coefficients to be ready
  while (timer-- && (!(reg & DPS310_BIT_SENSOR_RDY) || !(reg & DPS310_BIT_COEF_RDY))) {
    reg = this->read_byte(DPS310_REG_MEAS_CFG).value_or(0);
    delay(5);
  }

  if (!(reg & DPS310_BIT_SENSOR_RDY) || !(reg & DPS310_BIT_COEF_RDY)) {  // the flags were not set in time
    this->mark_failed();
    return;
  }
  // read device ID
  if (!this->read_byte(DPS310_REG_PROD_REV_ID, &this->prod_rev_id_)) {
    this->mark_failed();
    return;
  }
  // read in coefficients used to calculate the compensated pressure and temperature values
  if (!this->read_bytes(DPS310_REG_COEF, coef_data_raw, DPS310_NUM_COEF_REGS)) {
    this->mark_failed();
    return;
  }
  // read in coefficients source register, too -- we need this a few lines down
  if (!this->read_byte(DPS310_REG_TMP_COEF_SRC, &reg)) {
    this->mark_failed();
    return;
  }
  // set up operational stuff
  if (!this->write_byte(DPS310_REG_PRS_CFG, DPS310_VAL_PRS_CFG)) {
    this->mark_failed();
    return;
  }
  if (!this->write_byte(DPS310_REG_TMP_CFG, DPS310_VAL_TMP_CFG | (reg & DPS310_BIT_TMP_COEF_SRC))) {
    this->mark_failed();
    return;
  }
  if (!this->write_byte(DPS310_REG_CFG, DPS310_VAL_REG_CFG)) {
    this->mark_failed();
    return;
  }
  if (!this->write_byte(DPS310_REG_MEAS_CFG, 0x07)) {  // enable background mode
    this->mark_failed();
    return;
  }

  this->c0_ =  // we only ever use c0/2, so just divide by 2 here to save time later
      DPS310Component::twos_complement(
          int16_t(((uint16_t) coef_data_raw[0] << 4) | (((uint16_t) coef_data_raw[1] >> 4) & 0x0F)), 12) /
      2;

  this->c1_ =
      DPS310Component::twos_complement(int16_t((((uint16_t) coef_data_raw[1] & 0x0F) << 8) | coef_data_raw[2]), 12);

  this->c00_ = ((uint32_t) coef_data_raw[3] << 12) | ((uint32_t) coef_data_raw[4] << 4) |
               (((uint32_t) coef_data_raw[5] >> 4) & 0x0F);
  this->c00_ = DPS310Component::twos_complement(c00_, 20);

  this->c10_ =
      (((uint32_t) coef_data_raw[5] & 0x0F) << 16) | ((uint32_t) coef_data_raw[6] << 8) | (uint32_t) coef_data_raw[7];
  this->c10_ = DPS310Component::twos_complement(c10_, 20);

  this->c01_ = int16_t(((uint16_t) coef_data_raw[8] << 8) | (uint16_t) coef_data_raw[9]);
  this->c11_ = int16_t(((uint16_t) coef_data_raw[10] << 8) | (uint16_t) coef_data_raw[11]);
  this->c20_ = int16_t(((uint16_t) coef_data_raw[12] << 8) | (uint16_t) coef_data_raw[13]);
  this->c21_ = int16_t(((uint16_t) coef_data_raw[14] << 8) | (uint16_t) coef_data_raw[15]);
  this->c30_ = int16_t(((uint16_t) coef_data_raw[16] << 8) | (uint16_t) coef_data_raw[17]);
}

void DPS310Component::dump_config() {
  ESP_LOGCONFIG(TAG, "DPS310:");
  ESP_LOGCONFIG(TAG, "  Product ID: %u", this->prod_rev_id_ & 0x0F);
  ESP_LOGCONFIG(TAG, "  Revision ID: %u", (this->prod_rev_id_ >> 4) & 0x0F);
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with DPS310 failed!");
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
}

float DPS310Component::get_setup_priority() const { return setup_priority::DATA; }

void DPS310Component::update() {
  if (!this->update_in_progress_) {
    this->update_in_progress_ = true;
    this->read_();
  }
}

void DPS310Component::read_() {
  uint8_t reg = 0;
  if (!this->read_byte(DPS310_REG_MEAS_CFG, &reg)) {
    this->status_set_warning();
    return;
  }

  if ((!this->got_pres_) && (reg & DPS310_BIT_PRS_RDY)) {
    this->read_pressure_();
  }

  if ((!this->got_temp_) && (reg & DPS310_BIT_TMP_RDY)) {
    this->read_temperature_();
  }

  if (this->got_pres_ && this->got_temp_) {
    this->calculate_values_(this->raw_temperature_, this->raw_pressure_);
    this->got_pres_ = false;
    this->got_temp_ = false;
    this->update_in_progress_ = false;
    this->status_clear_warning();
  } else {
    auto f = std::bind(&DPS310Component::read_, this);
    this->set_timeout("dps310", 10, f);
  }
}

void DPS310Component::read_pressure_() {
  uint8_t bytes[3];
  if (!this->read_bytes(DPS310_REG_PRS_B2, bytes, 3)) {
    this->status_set_warning();
    return;
  }
  this->got_pres_ = true;
  this->raw_pressure_ = DPS310Component::twos_complement(
      int32_t((uint32_t(bytes[0]) << 16) | (uint32_t(bytes[1]) << 8) | (uint32_t(bytes[2]))), 24);
}

void DPS310Component::read_temperature_() {
  uint8_t bytes[3];
  if (!this->read_bytes(DPS310_REG_TMP_B2, bytes, 3)) {
    this->status_set_warning();
    return;
  }
  this->got_temp_ = true;
  this->raw_temperature_ = DPS310Component::twos_complement(
      int32_t((uint32_t(bytes[0]) << 16) | (uint32_t(bytes[1]) << 8) | (uint32_t(bytes[2]))), 24);
}

// Calculations are taken from the datasheet which can be found here:
// https://www.infineon.com/dgdl/Infineon-DPS310-DataSheet-v01_02-EN.pdf?fileId=5546d462576f34750157750826c42242
// Sections "How to Calculate Compensated Pressure Values" and "How to Calculate Compensated Temperature Values"
// Variable names below match variable names from the datasheet but lowercased
void DPS310Component::calculate_values_(int32_t raw_temperature, int32_t raw_pressure) {
  const float t_raw_sc = (float) raw_temperature / DPS310_SCALE_FACTOR;
  const float p_raw_sc = (float) raw_pressure / DPS310_SCALE_FACTOR;

  const float temperature = t_raw_sc * this->c1_ + this->c0_;  // c0/2 done earlier!

  const float pressure = (this->c00_ + p_raw_sc * (this->c10_ + p_raw_sc * (this->c20_ + p_raw_sc * this->c30_)) +
                          t_raw_sc * this->c01_ + t_raw_sc * p_raw_sc * (this->c11_ + p_raw_sc * this->c21_)) /
                         100;  // divide by 100 for hPa

  if (this->temperature_sensor_ != nullptr) {
    this->temperature_sensor_->publish_state(temperature);
  }
  if (this->pressure_sensor_ != nullptr) {
    this->pressure_sensor_->publish_state(pressure);
  }
}

int32_t DPS310Component::twos_complement(int32_t val, uint8_t bits) {
  if (val & ((uint32_t) 1 << (bits - 1))) {
    val -= (uint32_t) 1 << bits;
  }
  return val;
}

}  // namespace dps310
}  // namespace esphome
