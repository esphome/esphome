#include "sen0501.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace sen0501 {

static const char *const TAG = "sen0501";

// Device Address
static const uint8_t SEN050X_DEFAULT_DEVICE_ADDRESS = 0x22;
// Board identification
static const uint16_t DEVICE_PID_GRAVITY = 0x01F5;
static const uint16_t DEVICE_PID_BREAKOUT = 0x01F4;
static const uint16_t DEVICE_VID = 0x3343;
// Device registers
static const uint16_t REG_PID = 0x0000;
static const uint16_t REG_VID = 0x0002;
static const uint16_t REG_DEVICE_ADDR = 0x0004;
static const uint16_t REG_UART_CTRL0 = 0x0006;
static const uint16_t EG_UART_CTRL1 = 0x0008;
static const uint16_t REG_VERSION = 0x000A;
// Data registers
static const uint16_t REG_ULTRAVIOLET_INTENSITY = 0x0010;
static const uint16_t REG_LUMINOUS_INTENSITY = 0x0012;
static const uint16_t REG_TEMP = 0x0014;
static const uint16_t REG_HUMIDITY = 0x0016;
static const uint16_t REG_ATMOSPHERIC_PRESSURE = 0x0018;

// PUBLIC

void Sen0501Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up sen0501...");
  uint8_t product_id_first_byte;
  if (!this->read_byte(REG_PID, &product_id_first_byte)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  } else {
    uint8_t buf[2];
    this->read_bytes(REG_PID, buf, 2);
    uint16_t product_id = this->encode_uint16(buf[0], buf[1]);
    if ((product_id != DEVICE_PID_GRAVITY) && (product_id != DEVICE_PID_BREAKOUT)) {
      this->error_code_ = WRONG_DEVICE_ID;
      this->mark_failed();
      return;
    }
    this->read_bytes(REG_VID, buf, 2);
    uint16_t vendor_id = this->encode_uint16(buf[0], buf[1]);
    if (vendor_id != DEVICE_VID) {
      this->error_code_ = WRONG_VENDOR_ID;
      this->mark_failed();
      return;
    }
  }
}

void Sen0501Component::update() {
  this->read_temperature_();
  this->read_humidity_();
  this->read_uv_intensity_();
  this->read_luminous_intensity_();
  this->read_atmospheric_pressure_();
}

void Sen0501Component::dump_config() {
  ESP_LOGCONFIG(TAG, "DFRobot Environmental Sensor - sen0501:");
  LOG_I2C_DEVICE(this);
  switch (this->error_code_) {
    case COMMUNICATION_FAILED:
      ESP_LOGE(TAG, "Communication with sen0501 failed!");
      break;
    case WRONG_DEVICE_ID:
      ESP_LOGE(TAG, "Wrong device ID! Is it a sen0501?");
      break;
    case WRONG_VENDOR_ID:
      ESP_LOGE(TAG, "Wrong vendor ID! Is it made by DFRobot(Zhiwei Robotics Corp)?");
      break;
    case NONE:
    default:
      break;
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
  LOG_SENSOR("  ", "UV Intensity", this->uv_intensity_);
  LOG_SENSOR("  ", "Luminous Intensity", this->luminous_intensity_);
  LOG_SENSOR("  ", "Atmospheric Pressure", this->atmospheric_pressure_);
  LOG_SENSOR("  ", "Elevation", this->elevation_);
}

float Sen0501Component::get_setup_priority() const { return setup_priority::DATA; }

// PROTECTED

void Sen0501Component::read_temperature_() {
  if (this->temperature_ == nullptr)
    return;
  uint8_t buffer[2];
  float temp;
  this->read_bytes(REG_TEMP, buffer, 2);
  uint16_t data = this->encode_uint16(buffer[0], buffer[1]);
  temp = 175.0f * float(data) / 65536.0f - 45.0f;
  this->temperature_->publish_state(temp);
}

void Sen0501Component::read_humidity_() {
  if (this->humidity_ == nullptr)
    return;
  uint8_t buffer[2];
  float humidity;
  this->read_bytes(REG_HUMIDITY, buffer, 2);
  uint16_t data = this->encode_uint16(buffer[0], buffer[1]);
  humidity = (float) data * 100 / 65536;
  this->humidity_->publish_state(humidity);
}

void Sen0501Component::read_uv_intensity_() {
  if (this->uv_intensity_ == nullptr)
    return;
  uint8_t buffer[2];
  uint16_t version = 0;
  float ultra_violet;
  this->read_bytes(REG_VERSION, buffer, 2);
  version = this->encode_uint16(buffer[0], buffer[1]);
  if (version == 0x1001) {
    this->read_bytes(REG_ULTRAVIOLET_INTENSITY, buffer, 2);
    uint16_t uv_level = this->encode_uint16(buffer[0], buffer[1]);
    ultra_violet = (float) uv_level / 1800.0;
  } else {
    this->read_bytes(REG_ULTRAVIOLET_INTENSITY, buffer, 2);
    uint16_t uv_level = this->encode_uint16(buffer[0], buffer[1]);
    float output_voltage = 3.0 * uv_level / 1024;
    if (output_voltage <= 0.99) {
      output_voltage = 0.99;
    } else if (output_voltage >= 2.99) {
      output_voltage = 2.99;
    }
    ultra_violet = this->remap(output_voltage, 0.99f, 2.9f, 0.0f, 15.0f);
  }
  this->uv_intensity_->publish_state(ultra_violet);
}

void Sen0501Component::read_luminous_intensity_() {
  if (this->luminous_intensity_ == nullptr)
    return;
  uint8_t buffer[2];
  this->read_bytes(REG_LUMINOUS_INTENSITY, buffer, 2);
  uint16_t data = this->encode_uint16(buffer[0], buffer[1]);
  float luminous = data;
  luminous = luminous * (1.0023f + luminous * (8.1488e-5f + luminous * (-9.3924e-9f + luminous * 6.0135e-13f)));
  this->luminous_intensity_->publish_state(luminous);
}

void Sen0501Component::read_atmospheric_pressure_() {
  if (this->atmospheric_pressure_ == nullptr)
    return;
  uint8_t buffer[2];
  this->read_bytes(REG_ATMOSPHERIC_PRESSURE, buffer, 2);
  uint16_t atmosphere = this->encode_uint16(buffer[0], buffer[1]);
  this->atmospheric_pressure_->publish_state(atmosphere);
  if (this->elevation_ == nullptr)
    return;
  float elevation = 44330 * (1.0 - this->pow(atmosphere / 1015.0f, 0.1903));
  this->elevation_->publish_state(elevation);
}

}  // namespace sen0501
}  // namespace esphome
