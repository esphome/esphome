#include "as5048b.h"
#include "esphome/core/log.h"

namespace esphome {
namespace as5048b {
static const char *const TAG = "as5048b";

static const uint8_t AS5048_ADDRESS = 0x40;         // 0b10000 + ( A1 & A2 to GND)
static const uint8_t AS5048B_PROG_REG = 0x03;
static const uint8_t AS5048B_ADDR_REG = 0x15;
static const uint8_t AS5048B_ZEROMSB_REG = 0x16;    // bits 0..7
static const uint8_t AS5048B_ZEROLSB_REG = 0x17;    // bits 0..5
static const uint8_t AS5048B_GAIN_REG = 0xFA;
static const uint8_t AS5048B_DIAG_REG = 0xFB;
static const uint8_t AS5048B_MAGNMSB_REG = 0xFC;    // bits 0..7
static const uint8_t AS5048B_MAGNLSB_REG = 0xFD;    // bits 0..5
static const uint8_t AS5048B_ANGLMSB_REG = 0xFE;    // bits 0..7 = Angle<6>...Angle<13>
static const uint8_t AS5048B_ANGLLSB_REG = 0xFF;    // bits 0..5 = Angle<0>...Angle<5>
static const double AS5048B_RESOLUTION = 16384.0;   // 14 bits

static const uint8_t TC74_REGISTER_TEMPERATURE = 0x00;
static const uint8_t TC74_REGISTER_CONFIGURATION = 0x01;
static const uint8_t TC74_DATA_READY_MASK = 0x40;

void AS5048bComponent::setup() {}

void AS5048bComponent::update() { this->read_angle_(); }

void AS5048bComponent::dump_config() {
  LOG_SENSOR("", "AS5048b", this);
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Connection with AS5048b failed!");
  }
  LOG_UPDATE_INTERVAL(this);
}

void AS5048bComponent::read_angle_() {
  uint8_t angleMSB = 0;
  uint8_t angleLSB = 0;

  i2c::ErrorCode errorMSB = this->read_register(AS5048B_ANGLMSB_REG, &angleMSB, 2);

  i2c::ErrorCode errorLSB = this->read_register(AS5048B_ANGLLSB_REG, &angleLSB, 2);

  if (errorMSB || errorLSB) {
    ESP_LOGE(TAG, "Failed to read registers with codes %d and %d for MSB and LSB.", errorMSB, errorLSB);
    this->mark_failed();
    return;
  }

  uint16_t rawAngle = angleLSB << 6;
  rawAngle += (angleMSB & 0x3F);

  double angle = (rawAngle / AS5048B_RESOLUTION) * 360.0;

  ESP_LOGD(TAG, "Got angle %.2f °", angle);
  this->publish_state(angle);
  this->status_clear_warning();
}

float AS5048bComponent::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace as5048b
}  // namespace esphome
