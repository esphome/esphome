#include "mlx90614.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace mlx90614 {

static const char *const TAG = "mlx90614.sensor";

// MLX90614 registers from the datasheet
// RAM
static const uint8_t MLX90614_RAWIR1 = 0x04;
static const uint8_t MLX90614_RAWIR2 = 0x05;
static const uint8_t MLX90614_TA = 0x06;
static const uint8_t MLX90614_TOBJ1 = 0x07;
static const uint8_t MLX90614_TOBJ2 = 0x08;
// EEPROM
static const uint8_t MLX90614_TOMAX = 0x20;
static const uint8_t MLX90614_TOMIN = 0x21;
static const uint8_t MLX90614_PWMCTRL = 0x22;
static const uint8_t MLX90614_TARANGE = 0x23;
static const uint8_t MLX90614_EMISS = 0x24;
static const uint8_t MLX90614_CONFIG = 0x25;
static const uint8_t MLX90614_ADDR = 0x2E;
static const uint8_t MLX90614_ID1 = 0x3C;
static const uint8_t MLX90614_ID2 = 0x3D;
static const uint8_t MLX90614_ID3 = 0x3E;
static const uint8_t MLX90614_ID4 = 0x3F;

void MLX90614Component::setup() {
  ESP_LOGI(TAG, "Setting up MLX90614 sensor at I2C address 0x%02X", this->address_);
  uint8_t id[4];
  uint8_t reg = MLX90614_ID1;
  if (this->bus_->write_read(this->address_, &reg, 1, id, 4) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed I2C read during setup()");
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "ID: 0x%02X%02X%02X%02X", id[0], id[1], id[2], id[3]);
}

void MLX90614Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MLX90614:");
  LOG_I2C_DEVICE(this);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with MLX90614 failed earlier during setup");
    return;
  }
  LOG_UPDATE_INTERVAL(this);
  if (this->temperature_target_) {
    LOG_SENSOR("  ", "Target Temperature", this->temperature_target_);
  }
  if (this->temperature_reference_) {
    LOG_SENSOR("  ", "Reference Temperature", this->temperature_reference_);
  }
}

void MLX90614Component::update() {
  if (this->temperature_target_ != nullptr) {
    float objtemp = read_temp_register_(MLX90614_TOBJ1);
    this->temperature_target_->publish_state(objtemp);
  }
  if (this->temperature_reference_ != nullptr) {
    float ambtemp = read_temp_register_(MLX90614_TA);
    temperature_reference_->publish_state(ambtemp);
  }
  this->status_clear_warning();
}

// -------- Protected ---------------------------

float MLX90614Component::read_temp_register_(uint8_t reg) {
  uint8_t r[2];
  if (this->bus_->write_read(this->address_, &reg, 1, r, 2) != i2c::ERROR_OK) {
    this->status_set_warning();
  }
  float temp = (float) ((uint16_t) r[1] << 8 | r[0]);
  temp *= .02;
  temp -= 273.15;
  return temp;
}

}  // namespace mlx90614
}  // namespace esphome
