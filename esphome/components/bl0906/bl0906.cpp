#include "bl0906.h"
#include "constants.h"

#include "esphome/core/log.h"

namespace esphome {
namespace bl0906 {

static const char *const TAG = "bl0906";

constexpr uint32_t to_uint32_t(ube24_t input) { return input.h << 16 | input.m << 8 | input.l; }

constexpr int32_t to_int32_t(sbe24_t input) { return input.h << 16 | input.m << 8 | input.l; }

// The SUM byte is (Addr+Data_L+Data_M+Data_H)&0xFF negated;
constexpr uint8_t bl0906_checksum(const uint8_t address, const DataPacket *data) {
  return (address + data->l + data->m + data->h) ^ 0xFF;
}

void BL0906::loop() {
  if (this->current_channel_ == UINT8_MAX) {
    return;
  }

  while (this->available())
    this->flush();

  if (this->current_channel_ == 0) {
    // Temperature
    this->read_data_(BL0906_TEMPERATURE, BL0906_TREF, this->temperature_sensor_);
  } else if (this->current_channel_ == 1) {
    this->read_data_(BL0906_I_1_RMS, BL0906_IREF, this->current_1_sensor_);
    this->read_data_(BL0906_WATT_1, BL0906_PREF, this->power_1_sensor_);
    this->read_data_(BL0906_CF_1_CNT, BL0906_EREF, this->energy_1_sensor_);
  } else if (this->current_channel_ == 2) {
    this->read_data_(BL0906_I_2_RMS, BL0906_IREF, this->current_2_sensor_);
    this->read_data_(BL0906_WATT_2, BL0906_PREF, this->power_2_sensor_);
    this->read_data_(BL0906_CF_2_CNT, BL0906_EREF, this->energy_2_sensor_);
  } else if (this->current_channel_ == 3) {
    this->read_data_(BL0906_I_3_RMS, BL0906_IREF, this->current_3_sensor_);
    this->read_data_(BL0906_WATT_3, BL0906_PREF, this->power_3_sensor_);
    this->read_data_(BL0906_CF_3_CNT, BL0906_EREF, this->energy_3_sensor_);
  } else if (this->current_channel_ == 4) {
    this->read_data_(BL0906_I_4_RMS, BL0906_IREF, this->current_4_sensor_);
    this->read_data_(BL0906_WATT_4, BL0906_PREF, this->power_4_sensor_);
    this->read_data_(BL0906_CF_4_CNT, BL0906_EREF, this->energy_4_sensor_);
  } else if (this->current_channel_ == 5) {
    this->read_data_(BL0906_I_5_RMS, BL0906_IREF, this->current_5_sensor_);
    this->read_data_(BL0906_WATT_5, BL0906_PREF, this->power_5_sensor_);
    this->read_data_(BL0906_CF_5_CNT, BL0906_EREF, this->energy_5_sensor_);
  } else if (this->current_channel_ == 6) {
    this->read_data_(BL0906_I_6_RMS, BL0906_IREF, this->current_6_sensor_);
    this->read_data_(BL0906_WATT_6, BL0906_PREF, this->power_6_sensor_);
    this->read_data_(BL0906_CF_6_CNT, BL0906_EREF, this->energy_6_sensor_);
  } else if (this->current_channel_ == UINT8_MAX - 2) {
    // Frequency
    this->read_data_(BL0906_FREQUENCY, BL0906_FREF, frequency_sensor_);
    // Voltage
    this->read_data_(BL0906_V_RMS, BL0906_UREF, voltage_sensor_);
  } else if (this->current_channel_ == UINT8_MAX - 1) {
    // Total power
    this->read_data_(BL0906_WATT_SUM, BL0906_WATT, this->total_power_sensor_);
    // Total Energy
    this->read_data_(BL0906_CF_SUM_CNT, BL0906_CF, this->total_energy_sensor_);
  } else {
    this->current_channel_ = UINT8_MAX - 2;  // Go to frequency and voltage
    return;
  }
  this->current_channel_++;
  this->handle_actions_();
}

void BL0906::setup() {
  while (this->available())
    this->flush();
  this->write_array(USR_WRPROT_WITABLE, sizeof(USR_WRPROT_WITABLE));
  // Calibration (1: register address; 2: value before calibration; 3: value after calibration)
  this->bias_correction_(BL0906_RMSOS_1, 0.01600, 0);  // Calibration current_1
  this->bias_correction_(BL0906_RMSOS_2, 0.01500, 0);
  this->bias_correction_(BL0906_RMSOS_3, 0.01400, 0);
  this->bias_correction_(BL0906_RMSOS_4, 0.01300, 0);
  this->bias_correction_(BL0906_RMSOS_5, 0.01200, 0);
  this->bias_correction_(BL0906_RMSOS_6, 0.01200, 0);  // Calibration current_6

  this->write_array(USR_WRPROT_ONLYREAD, sizeof(USR_WRPROT_ONLYREAD));
}

void BL0906::update() { this->current_channel_ = 0; }

size_t BL0906::enqueue_action_(ActionCallbackFuncPtr function) {
  this->action_queue_.push_back(function);
  return this->action_queue_.size();
}

void BL0906::handle_actions_() {
  if (this->action_queue_.empty()) {
    return;
  }
  ActionCallbackFuncPtr ptr_func = nullptr;
  for (int i = 0; i < this->action_queue_.size(); i++) {
    ptr_func = this->action_queue_[i];
    if (ptr_func) {
      ESP_LOGI(TAG, "HandleActionCallback[%d]...", i);
      (this->*ptr_func)();
    }
  }

  while (this->available()) {
    this->read();
  }

  this->action_queue_.clear();
}

// Reset energy
void BL0906::reset_energy_() {
  this->write_array(BL0906_INIT[0], 6);
  delay(1);
  this->flush();

  ESP_LOGW(TAG, "RMSOS:%02X%02X%02X%02X%02X%02X", BL0906_INIT[0][0], BL0906_INIT[0][1], BL0906_INIT[0][2],
           BL0906_INIT[0][3], BL0906_INIT[0][4], BL0906_INIT[0][5]);
}

// Read data
void BL0906::read_data_(const uint8_t address, const float reference, sensor::Sensor *sensor) {
  if (sensor == nullptr) {
    return;
  }
  DataPacket buffer;
  ube24_t data_u24;
  sbe24_t data_s24;
  float value = 0;

  bool signed_result = reference == BL0906_TREF || reference == BL0906_WATT || reference == BL0906_PREF;

  this->write_byte(BL0906_READ_COMMAND);
  this->write_byte(address);
  if (this->read_array((uint8_t *) &buffer, sizeof(buffer) - 1)) {
    if (bl0906_checksum(address, &buffer) == buffer.checksum) {
      if (signed_result) {
        data_s24.l = buffer.l;
        data_s24.m = buffer.m;
        data_s24.h = buffer.h;
      } else {
        data_u24.l = buffer.l;
        data_u24.m = buffer.m;
        data_u24.h = buffer.h;
      }
    } else {
      ESP_LOGW(TAG, "Junk on wire. Throwing away partial message");
      while (read() >= 0)
        ;
      return;
    }
  }
  // Power
  if (reference == BL0906_PREF) {
    value = (float) to_int32_t(data_s24) * reference;
  }

  // Total power
  if (reference == BL0906_WATT) {
    value = (float) to_int32_t(data_s24) * reference;
  }

  // Voltage, current, power, total power
  if (reference == BL0906_UREF || reference == BL0906_IREF || reference == BL0906_EREF || reference == BL0906_CF) {
    value = (float) to_uint32_t(data_u24) * reference;
  }

  // Frequency
  if (reference == BL0906_FREF) {
    value = reference / (float) to_uint32_t(data_u24);
  }
  // Chip temperature
  if (reference == BL0906_TREF) {
    value = (float) to_int32_t(data_s24);
    value = (value - 64) * 12.5 / 59 - 40;
  }
  sensor->publish_state(value);
}

// RMS offset correction
void BL0906::bias_correction_(uint8_t address, float measurements, float correction) {
  DataPacket data;
  float ki = 12875 * 1 * (5.1 + 5.1) * 1000 / 2000 / 1.097;  // Current coefficient
  float i_rms0 = measurements * ki;
  float i_rms = correction * ki;
  int32_t value = (i_rms * i_rms - i_rms0 * i_rms0) / 256;
  data.l = value << 24 >> 24;
  data.m = value << 16 >> 24;
  if (value < 0) {
    data.h = (value << 8 >> 24) | 0b10000000;
  }
  data.address = bl0906_checksum(address, &data);
  ESP_LOGV(TAG, "RMSOS:%02X%02X%02X%02X%02X%02X", BL0906_WRITE_COMMAND, address, data.l, data.m, data.h, data.address);
  this->write_byte(BL0906_WRITE_COMMAND);
  this->write_byte(address);
  this->write_byte(data.l);
  this->write_byte(data.m);
  this->write_byte(data.h);
  this->write_byte(data.address);
}

void BL0906::dump_config() {
  ESP_LOGCONFIG(TAG, "BL0906:");
  LOG_SENSOR("  ", "Voltage", this->voltage_sensor_);

  LOG_SENSOR("  ", "Current1", this->current_1_sensor_);
  LOG_SENSOR("  ", "Current2", this->current_2_sensor_);
  LOG_SENSOR("  ", "Current3", this->current_3_sensor_);
  LOG_SENSOR("  ", "Current4", this->current_4_sensor_);
  LOG_SENSOR("  ", "Current5", this->current_5_sensor_);
  LOG_SENSOR("  ", "Current6", this->current_6_sensor_);

  LOG_SENSOR("  ", "Power1", this->power_1_sensor_);
  LOG_SENSOR("  ", "Power2", this->power_2_sensor_);
  LOG_SENSOR("  ", "Power3", this->power_3_sensor_);
  LOG_SENSOR("  ", "Power4", this->power_4_sensor_);
  LOG_SENSOR("  ", "Power5", this->power_5_sensor_);
  LOG_SENSOR("  ", "Power6", this->power_6_sensor_);

  LOG_SENSOR("  ", "Energy1", this->energy_1_sensor_);
  LOG_SENSOR("  ", "Energy2", this->energy_2_sensor_);
  LOG_SENSOR("  ", "Energy3", this->energy_3_sensor_);
  LOG_SENSOR("  ", "Energy4", this->energy_4_sensor_);
  LOG_SENSOR("  ", "Energy5", this->energy_5_sensor_);
  LOG_SENSOR("  ", "Energy6", this->energy_6_sensor_);

  LOG_SENSOR("  ", "Total Power", this->total_power_sensor_);
  LOG_SENSOR("  ", "Total Energy", this->total_energy_sensor_);
  LOG_SENSOR("  ", "Frequency", this->frequency_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
}

}  // namespace bl0906
}  // namespace esphome
