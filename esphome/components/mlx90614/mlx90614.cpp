#include "mlx90614.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace mlx90614 {

static const char *const TAG = "mlx90614.sensor";

// MLX90614 registers from the datasheet
// RAM
#define MLX90614_RAWIR1 0x04
#define MLX90614_RAWIR2 0x05
#define MLX90614_TA 0x06
#define MLX90614_TOBJ1 0x07
#define MLX90614_TOBJ2 0x08
// EEPROM
#define MLX90614_TOMAX 0x20
#define MLX90614_TOMIN 0x21
#define MLX90614_PWMCTRL 0x22
#define MLX90614_TARANGE 0x23
#define MLX90614_EMISS 0x24
#define MLX90614_CONFIG 0x25
#define MLX90614_ADDR 0x2E
#define MLX90614_ID1 0x3C
#define MLX90614_ID2 0x3D
#define MLX90614_ID3 0x3E
#define MLX90614_ID4 0x3F

void MLX90614Component::setup() {
  uint8_t address = this->address_;
  ESP_LOGI(TAG, "Setting up MLX90614 sensor at I2C address 0x%02X", address);
  uint8_t id;
  if (!this->read_byte(MLX90614_TA, &id)) {
    ESP_LOGE(TAG, "Failed I2C read during setup()");
    this->mark_failed();
    return;
  }
}

void MLX90614Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MLX90614:");
  LOG_I2C_DEVICE(this);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with MLX90614 failed earlier, during setup");
    return;
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Target Temperature", this->temperature_target_);
  LOG_SENSOR("  ", "Reference Temperature", this->temperature_reference_);
}

void MLX90614Component::update() {
  if (this->temperature_target_ != nullptr) {
    float objtemp = read_temp_register_(MLX90614_TOBJ1);
    this->temperature_target_->publish_state(objtemp);
  } 
  if (this->this->temperature_reference_ != nullptr) {
    float ambtemp = read_temp_register_(MLX90614_TA);
    temperature_reference_->publish_state(ambtemp);
  }
  this->status_clear_warning();
}

// -------- Protected ---------------------------

float MLX90614Component::read_temp_register_(uint8_t reg) {
  uint16_t raw;
  if (this->write(&reg, 1) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  delay(50);  // NOLINT
  if (this->read(reinterpret_cast<uint8_t *>(&raw), 2) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  raw = i2c::i2ctohs(raw);
  // Bad reading if MSB of raw is 1. TODO: Check for errors?
  float temp;
  temp = (float) raw;
  temp *= .02;
  temp -= 273.15;
  return temp;
}

}  // namespace mlx90614
}  // namespace esphome
