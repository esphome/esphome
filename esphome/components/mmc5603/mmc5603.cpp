#include "mmc5603.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mmc5603 {

static const char *const TAG = "mmc5603";
static const uint8_t MMC5603_ADDRESS = 0x30;
static const uint8_t MMC56X3_PRODUCT_ID = 0x39;

static const uint8_t MMC56X3_DEFAULT_ADDRESS = 0x30;
static const uint8_t MMC56X3_CHIP_ID = 0x10;

static const uint8_t MMC56X3_ADDR_XOUT0 = 0x00;
static const uint8_t MMC56X3_ADDR_XOUT1 = 0x01;
static const uint8_t MMC56X3_ADDR_XOUT2 = 0x06;

static const uint8_t MMC56X3_ADDR_YOUT0 = 0x02;
static const uint8_t MMC56X3_ADDR_YOUT1 = 0x03;
static const uint8_t MMC56X3_ADDR_YOUT2 = 0x07;

static const uint8_t MMC56X3_ADDR_ZOUT0 = 0x04;
static const uint8_t MMC56X3_ADDR_ZOUT1 = 0x05;
static const uint8_t MMC56X3_ADDR_ZOUT2 = 0x08;

static const uint8_t MMC56X3_OUT_TEMP = 0x09;
static const uint8_t MMC56X3_STATUS_REG = 0x18;
static const uint8_t MMC56X3_CTRL0_REG = 0x1B;
static const uint8_t MMC56X3_CTRL1_REG = 0x1C;
static const uint8_t MMC56X3_CTRL2_REG = 0x1D;
static const uint8_t MMC5603_ODR_REG = 0x1A;

void MMC5603Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MMC5603...");
  uint8_t id = 0;
  if (!this->read_byte(MMC56X3_PRODUCT_ID, &id)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }

  if (id != MMC56X3_CHIP_ID) {
    ESP_LOGCONFIG(TAG, "Chip Wrong");
    this->error_code_ = ID_REGISTERS;
    this->mark_failed();
    return;
  }

  if (!this->write_byte(MMC56X3_CTRL1_REG, 0x80)) {  // turn on set bit
    ESP_LOGCONFIG(TAG, "Control 1 Failed for set bit");
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }

  if (!this->write_byte(MMC56X3_CTRL0_REG, 0x08)) {  // turn on set bit
    ESP_LOGCONFIG(TAG, "Control 0 Failed for set bit");
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }

  if (!this->write_byte(MMC56X3_CTRL0_REG, 0x10)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }

  uint8_t ctrl_2 = 0;

  ctrl_2 &= ~0x10;  // turn off cmm_en bit
  if (!this->write_byte(MMC56X3_CTRL2_REG, ctrl_2)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }
}
void MMC5603Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MMC5603:");
  LOG_I2C_DEVICE(this);
  if (this->error_code_ == COMMUNICATION_FAILED) {
    ESP_LOGE(TAG, "Communication with MMC5603 failed!");
  } else if (this->error_code_ == ID_REGISTERS) {
    ESP_LOGE(TAG, "The ID registers don't match - Is this really an MMC5603?");
  }
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "X Axis", this->x_sensor_);
  LOG_SENSOR("  ", "Y Axis", this->y_sensor_);
  LOG_SENSOR("  ", "Z Axis", this->z_sensor_);
  LOG_SENSOR("  ", "Heading", this->heading_sensor_);
}

float MMC5603Component::get_setup_priority() const { return setup_priority::DATA; }

void MMC5603Component::update() {
  if (!this->write_byte(MMC56X3_CTRL0_REG, 0x01)) {
    this->status_set_warning();
    return;
  }
  uint8_t status = 0;
  if (!this->read_byte(MMC56X3_STATUS_REG, &status)) {
    this->status_set_warning();
    return;
  }

  uint8_t buffer[9] = {0};

  if (!this->read_byte(MMC56X3_ADDR_XOUT0, &buffer[0]) || !this->read_byte(MMC56X3_ADDR_XOUT1, &buffer[1]) ||
      !this->read_byte(MMC56X3_ADDR_XOUT2, &buffer[2])) {
    this->status_set_warning();
    return;
  }

  if (!this->read_byte(MMC56X3_ADDR_YOUT0, &buffer[3]) || !this->read_byte(MMC56X3_ADDR_YOUT1, &buffer[4]) ||
      !this->read_byte(MMC56X3_ADDR_YOUT2, &buffer[5])) {
    this->status_set_warning();
    return;
  }

  if (!this->read_byte(MMC56X3_ADDR_ZOUT0, &buffer[6]) || !this->read_byte(MMC56X3_ADDR_ZOUT1, &buffer[7]) ||
      !this->read_byte(MMC56X3_ADDR_ZOUT2, &buffer[8])) {
    this->status_set_warning();
    return;
  }

  int32_t raw_x = 0;
  raw_x |= buffer[0] << 12;
  raw_x |= buffer[1] << 4;
  raw_x |= buffer[2] << 0;

  const float x = 0.0625 * (raw_x - 524288);

  int32_t raw_y = 0;
  raw_y |= buffer[3] << 12;
  raw_y |= buffer[4] << 4;
  raw_y |= buffer[5] << 0;

  const float y = 0.0625 * (raw_y - 524288);

  int32_t raw_z = 0;
  raw_z |= buffer[6] << 12;
  raw_z |= buffer[7] << 4;
  raw_z |= buffer[8] << 0;

  const float z = 0.0625 * (raw_z - 524288);

  const float heading = atan2f(0.0f - x, y) * 180.0f / M_PI;
  ESP_LOGD(TAG, "Got x=%0.02fµT y=%0.02fµT z=%0.02fµT heading=%0.01f°", x, y, z, heading);

  if (this->x_sensor_ != nullptr)
    this->x_sensor_->publish_state(x);
  if (this->y_sensor_ != nullptr)
    this->y_sensor_->publish_state(y);
  if (this->z_sensor_ != nullptr)
    this->z_sensor_->publish_state(z);
  if (this->heading_sensor_ != nullptr)
    this->heading_sensor_->publish_state(heading);
}

}  // namespace mmc5603
}  // namespace esphome
