// See https://github.com/sparkfun/SparkFun_MMC5983MA_Magnetometer_Arduino_Library/tree/main
// for datasheets and an Arduino implementation.

#include "mmc5983.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mmc5983 {

static const char *const TAG = "mmc5983";

namespace {
constexpr uint8_t IC0_ADDR = 0x09;
constexpr uint8_t IC1_ADDR = 0x0a;
constexpr uint8_t IC2_ADDR = 0x0b;
constexpr uint8_t IC3_ADDR = 0x0c;
constexpr uint8_t PRODUCT_ID_ADDR = 0x2f;

float convert_data_to_millitesla(uint8_t data_17_10, uint8_t data_9_2, uint8_t data_1_0) {
  int32_t counts = (data_17_10 << 10) | (data_9_2 << 2) | data_1_0;
  counts -= 131072;  // "Null Field Output" from datasheet.

  // Sensitivity is 16384 counts/gauss, which is 163840 counts/mT.
  return counts / 163840.0f;
}
}  // namespace

void MMC5983Component::update() {
  // Schedule a SET/RESET. This will recalibrate the sensor.
  // We are supposed to be able to set this once, and have it automatically continue every reading, but
  // this does not appear to work in continuous mode, even with En_prd_set turned on in Internal Control 2.
  // Bit 5 = Auto_SR_en (automatic SET/RESET enable).
  const uint8_t ic0_value = 0b10000;
  i2c::ErrorCode err = this->write_register(IC0_ADDR, &ic0_value, 1);
  if (err != i2c::ErrorCode::ERROR_OK) {
    ESP_LOGW(TAG, "Writing Internal Control 0 failed with i2c error %d", err);
    this->status_set_warning();
  }

  // Read out the data, 7 bytes starting from 0x00.
  uint8_t data[7];
  err = this->read_register(0x00, data, sizeof(data));
  if (err != i2c::ErrorCode::ERROR_OK) {
    ESP_LOGW(TAG, "Reading data failed with i2c error %d", err);
    this->status_set_warning();
    return;
  }

  // Unpack the data and publish to sensors.
  // Data is in this format:
  //   data[0]: Xout[17:10]
  //   data[1]: Xout[9:2]
  //   data[2]: Yout[17:10]
  //   data[3]: Yout[9:2]
  //   data[4]: Zout[17:10]
  //   data[5]: Zout[9:2]
  //   data[6]: { Xout[1], Xout[0], Yout[1], Yout[0], Zout[1], Zout[0], 0, 0 }
  if (this->x_sensor_) {
    this->x_sensor_->publish_state(convert_data_to_millitesla(data[0], data[1], (data[6] & 0b11000000) >> 6));
  }
  if (this->y_sensor_) {
    this->y_sensor_->publish_state(convert_data_to_millitesla(data[2], data[3], (data[6] & 0b00110000) >> 4));
  }
  if (this->z_sensor_) {
    this->z_sensor_->publish_state(convert_data_to_millitesla(data[4], data[5], (data[6] & 0b00001100) >> 2));
  }
}

void MMC5983Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MMC5983...");

  // Verify product id.
  const uint8_t mmc5983_product_id = 0x30;
  uint8_t id;
  i2c::ErrorCode err = this->read_register(PRODUCT_ID_ADDR, &id, 1);
  if (err != i2c::ErrorCode::ERROR_OK) {
    ESP_LOGE(TAG, "Reading product id failed with i2c error %d", err);
    this->mark_failed();
    return;
  }
  if (id != mmc5983_product_id) {
    ESP_LOGE(TAG, "Product id 0x%02x does not match expected value 0x%02x", id, mmc5983_product_id);
    this->mark_failed();
    return;
  }

  // Initialize Internal Control registers to 0.
  // Internal Control 0.
  const uint8_t zero = 0;
  err = this->write_register(IC0_ADDR, &zero, 1);
  if (err != i2c::ErrorCode::ERROR_OK) {
    ESP_LOGE(TAG, "Initializing Internal Control 0 failed with i2c error %d", err);
    this->mark_failed();
    return;
  }
  // Internal Control 1.
  err = this->write_register(IC1_ADDR, &zero, 1);
  if (err != i2c::ErrorCode::ERROR_OK) {
    ESP_LOGE(TAG, "Initializing Internal Control 1 failed with i2c error %d", err);
    this->mark_failed();
    return;
  }
  // Internal Control 2.
  err = this->write_register(IC2_ADDR, &zero, 1);
  if (err != i2c::ErrorCode::ERROR_OK) {
    ESP_LOGE(TAG, "Initializing Internal Control 2 failed with i2c error %d", err);
    this->mark_failed();
    return;
  }
  // Internal Control 3.
  err = this->write_register(IC3_ADDR, &zero, 1);
  if (err != i2c::ErrorCode::ERROR_OK) {
    ESP_LOGE(TAG, "Initializing Internal Control 3 failed with i2c error %d", err);
    this->mark_failed();
    return;
  }

  // Enable continuous mode at 100 Hz, using Internal Control 2.
  // Bit 3 = Cmm_en (continuous mode enable).
  // Bit [2:0] = Cm_freq. 0b101 = 100 Hz, the fastest reading speed at Bandwidth=100 Hz.
  const uint8_t ic2_value = 0b00001101;
  err = this->write_register(IC2_ADDR, &ic2_value, 1);
  if (err != i2c::ErrorCode::ERROR_OK) {
    ESP_LOGE(TAG, "Writing Internal Control 2 failed with i2c error %d", err);
    this->mark_failed();
    return;
  }
}

void MMC5983Component::dump_config() {
  ESP_LOGD(TAG, "MMC5983:");
  LOG_I2C_DEVICE(this);
  LOG_SENSOR("  ", "X", this->x_sensor_);
  LOG_SENSOR("  ", "Y", this->y_sensor_);
  LOG_SENSOR("  ", "Z", this->z_sensor_);
}

float MMC5983Component::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace mmc5983
}  // namespace esphome
