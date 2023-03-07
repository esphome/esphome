// ENS210 relative humidity and temperature sensor with I2C interface from ScioSense
//
// Datasheet: https://www.sciosense.com/wp-content/uploads/2021/01/ENS210.pdf
//
// Implementation based on:
//   https://github.com/maarten-pennings/ENS210
//   https://github.com/sciosense/ENS210_driver

#include "ens210.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace ens210 {

static const char *const TAG = "ens210";

// ENS210 chip constants
static const uint8_t ENS210_BOOTING_MS = 2;  // Booting time in ms (also after reset, or going to high power)
static const uint8_t ENS210_SINGLE_MEASURMENT_CONVERSION_TIME_MS =
    130;                                        // Conversion time in ms for single shot T/H measurement
static const uint16_t ENS210_PART_ID = 0x0210;  // The expected part id of the ENS210

// Addresses of the ENS210 registers
static const uint8_t ENS210_REGISTER_PART_ID = 0x00;
static const uint8_t ENS210_REGISTER_UID = 0x04;
static const uint8_t ENS210_REGISTER_SYS_CTRL = 0x10;
static const uint8_t ENS210_REGISTER_SYS_STAT = 0x11;
static const uint8_t ENS210_REGISTER_SENS_RUN = 0x21;
static const uint8_t ENS210_REGISTER_SENS_START = 0x22;
static const uint8_t ENS210_REGISTER_SENS_STOP = 0x23;
static const uint8_t ENS210_REGISTER_SENS_STAT = 0x24;
static const uint8_t ENS210_REGISTER_T_VAL = 0x30;
static const uint8_t ENS210_REGISTER_H_VAL = 0x33;

// CRC-7 constants
static const uint8_t CRC7_WIDTH = 7;    // A 7 bits CRC has polynomial of 7th order, which has 8 terms
static const uint8_t CRC7_POLY = 0x89;  // The 8 coefficients of the polynomial
static const uint8_t CRC7_IVEC = 0x7F;  // Initial vector has all 7 bits high

// Payload data constants
static const uint8_t DATA7_WIDTH = 17;
static const uint32_t DATA7_MASK = ((1UL << DATA7_WIDTH) - 1);  // 0b 0 1111 1111 1111 1111
static const uint32_t DATA7_MSB = (1UL << (DATA7_WIDTH - 1));   // 0b 1 0000 0000 0000 0000

// Converts a status to a human readable string
static const LogString *ens210_status_to_human(int status) {
  switch (status) {
    case ENS210Component::ENS210_STATUS_I2C_ERROR:
      return LOG_STR("I2C error - communication with ENS210 failed!");
    case ENS210Component::ENS210_STATUS_CRC_ERROR:
      return LOG_STR("CRC error");
    case ENS210Component::ENS210_STATUS_INVALID:
      return LOG_STR("Invalid data");
    case ENS210Component::ENS210_STATUS_OK:
      return LOG_STR("Status OK");
    case ENS210Component::ENS210_WRONG_CHIP_ID:
      return LOG_STR("ENS210 has wrong chip ID! Is it a ENS210?");
    default:
      return LOG_STR("Unknown");
  }
}

// Compute the CRC-7 of 'value' (should only have 17 bits)
// https://en.wikipedia.org/wiki/Cyclic_redundancy_check#Computation
static uint32_t crc7(uint32_t value) {
  // Setup polynomial
  uint32_t polynomial = CRC7_POLY;
  // Align polynomial with data
  polynomial = polynomial << (DATA7_WIDTH - CRC7_WIDTH - 1);
  // Loop variable (indicates which bit to test, start with highest)
  uint32_t bit = DATA7_MSB;
  // Make room for CRC value
  value = value << CRC7_WIDTH;
  bit = bit << CRC7_WIDTH;
  polynomial = polynomial << CRC7_WIDTH;
  // Insert initial vector
  value |= CRC7_IVEC;
  // Apply division until all bits done
  while (bit & (DATA7_MASK << CRC7_WIDTH)) {
    if (bit & value)
      value ^= polynomial;
    bit >>= 1;
    polynomial >>= 1;
  }
  return value;
}

void ENS210Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ENS210...");
  uint8_t data[2];
  uint16_t part_id = 0;
  // Reset
  if (!this->write_byte(ENS210_REGISTER_SYS_CTRL, 0x80)) {
    this->write_byte(ENS210_REGISTER_SYS_CTRL, 0x80);
    this->error_code_ = ENS210_STATUS_I2C_ERROR;
    this->mark_failed();
    return;
  }
  // Wait to boot after reset
  delay(ENS210_BOOTING_MS);
  // Must disable low power to read PART_ID
  if (!set_low_power_(false)) {
    // Try to go back to default mode (low power enabled)
    set_low_power_(true);
    this->error_code_ = ENS210_STATUS_I2C_ERROR;
    this->mark_failed();
    return;
  }
  // Read the PART_ID
  if (!this->read_bytes(ENS210_REGISTER_PART_ID, data, 2)) {
    // Try to go back to default mode (low power enabled)
    set_low_power_(true);
    this->error_code_ = ENS210_STATUS_I2C_ERROR;
    this->mark_failed();
    return;
  }
  // Pack bytes into partid
  part_id = data[1] * 256U + data[0] * 1U;
  // Check expected part id of the ENS210
  if (part_id != ENS210_PART_ID) {
    this->error_code_ = ENS210_WRONG_CHIP_ID;
    this->mark_failed();
  }
  // Set default power mode (low power enabled)
  set_low_power_(true);
}

void ENS210Component::dump_config() {
  ESP_LOGCONFIG(TAG, "ENS210:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "%s", LOG_STR_ARG(ens210_status_to_human(this->error_code_)));
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
}

float ENS210Component::get_setup_priority() const { return setup_priority::DATA; }

void ENS210Component::update() {
  // Execute a single measurement
  if (!this->write_byte(ENS210_REGISTER_SENS_RUN, 0x00)) {
    ESP_LOGE(TAG, "Starting single measurement failed!");
    this->status_set_warning();
    return;
  }
  // Trigger measurement
  if (!this->write_byte(ENS210_REGISTER_SENS_START, 0x03)) {
    ESP_LOGE(TAG, "Trigger of measurement failed!");
    this->status_set_warning();
    return;
  }
  // Wait for measurement to complete
  this->set_timeout("data", uint32_t(ENS210_SINGLE_MEASURMENT_CONVERSION_TIME_MS), [this]() {
    int temperature_data, temperature_status, humidity_data, humidity_status;
    uint8_t data[6];
    uint32_t h_val_data, t_val_data;
    // Set default status for early bail out
    temperature_status = ENS210_STATUS_I2C_ERROR;
    humidity_status = ENS210_STATUS_I2C_ERROR;

    // Read T_VAL and H_VAL
    if (!this->read_bytes(ENS210_REGISTER_T_VAL, data, 6)) {
      ESP_LOGE(TAG, "Communication with ENS210 failed!");
      this->status_set_warning();
      return;
    }
    // Pack bytes for humidity
    h_val_data = (uint32_t)((uint32_t) data[5] << 16 | (uint32_t) data[4] << 8 | (uint32_t) data[3]);
    // Extract humidity data and update the status
    extract_measurement_(h_val_data, &humidity_data, &humidity_status);

    if (humidity_status == ENS210_STATUS_OK) {
      if (this->humidity_sensor_ != nullptr) {
        float humidity = (humidity_data & 0xFFFF) / 512.0;
        this->humidity_sensor_->publish_state(humidity);
      }
    } else {
      ESP_LOGW(TAG, "Humidity status failure: %s", LOG_STR_ARG(ens210_status_to_human(humidity_status)));
      this->status_set_warning();
      return;
    }
    // Pack bytes for temperature
    t_val_data = (uint32_t)((uint32_t) data[2] << 16 | (uint32_t) data[1] << 8 | (uint32_t) data[0]);
    // Extract temperature data and update the status
    extract_measurement_(t_val_data, &temperature_data, &temperature_status);

    if (temperature_status == ENS210_STATUS_OK) {
      if (this->temperature_sensor_ != nullptr) {
        // Temperature in Celsius
        float temperature = (temperature_data & 0xFFFF) / 64.0 - 27315L / 100.0;
        this->temperature_sensor_->publish_state(temperature);
      }
    } else {
      ESP_LOGW(TAG, "Temperature status failure: %s", LOG_STR_ARG(ens210_status_to_human(temperature_status)));
    }
  });
}

// Extracts measurement 'data' and 'status' from a 'val' obtained from measurement.
void ENS210Component::extract_measurement_(uint32_t val, int *data, int *status) {
  *data = (val >> 0) & 0xffff;
  int valid = (val >> 16) & 0x1;
  uint32_t crc = (val >> 17) & 0x7f;
  uint32_t payload = (val >> 0) & 0x1ffff;
  // Check CRC
  uint8_t crc_ok = crc7(payload) == crc;

  if (!crc_ok) {
    *status = ENS210_STATUS_CRC_ERROR;
  } else if (!valid) {
    *status = ENS210_STATUS_INVALID;
  } else {
    *status = ENS210_STATUS_OK;
  }
}

// Sets ENS210 to low (true) or high (false) power. Returns false on I2C problems.
bool ENS210Component::set_low_power_(bool enable) {
  uint8_t low_power_cmd = enable ? 0x01 : 0x00;
  ESP_LOGD(TAG, "Enable low power: %s", enable ? "true" : "false");
  bool result = this->write_byte(ENS210_REGISTER_SYS_CTRL, low_power_cmd);
  delay(ENS210_BOOTING_MS);
  return result;
}

}  // namespace ens210
}  // namespace esphome
