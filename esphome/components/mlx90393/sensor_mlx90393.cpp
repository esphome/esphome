#include "sensor_mlx90393.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mlx90393 {

static const char *const TAG = "mlx90393";

bool MLX90393_cls::transceive(uint8_t *request, size_t request_size, uint8_t *response, size_t response_size) {
  i2c::ErrorCode e = this->write(request, request_size);
  if (e != i2c::ErrorCode::ERROR_OK) {
    return false;
  }
  e = this->read(response, response_size);
  if (e != i2c::ErrorCode::ERROR_OK) {
    return false;
  }
  return true;
}

void MLX90393_cls::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MLX90393_cls...");
  if (this->drdy_pin_ == nullptr) {
    mlx.begin_custom_i2c(this, 0, 0, -1);
  } else {
    // mlx.begin(0,0, this->drdy_pin_);
    // mlx.begin(0, 0);
    ESP_LOGE(TAG, "drdy pin not yet supported");
    this->mark_failed();
    return;
  }
  mlx.setGainSel(this->gain_);

  mlx.setResolution(this->resolutions_[0], this->resolutions_[1], this->resolutions_[2]);

  mlx.setOverSampling(this->osr_);

  mlx.setDigitalFiltering(this->filter_);
}

void MLX90393_cls::dump_config() {
  ESP_LOGCONFIG(TAG, "MLX90393_cls:");
  LOG_I2C_DEVICE(this);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with MLX90393_cls failed!");
    return;
  }
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "X Axis", this->x_sensor_);
  LOG_SENSOR("  ", "Y Axis", this->y_sensor_);
  LOG_SENSOR("  ", "Z Axis", this->z_sensor_);
}

float MLX90393_cls::get_setup_priority() const { return setup_priority::DATA; }

void MLX90393_cls::update() {
  MLX90393::txyz data;

  if (mlx.readData(data) == MLX90393::STATUS_OK) {
    ESP_LOGD(TAG, "received %f %f %f", data.x, data.y, data.z);
    if (this->x_sensor_ != nullptr) {
      this->x_sensor_->publish_state(data.x);
    }
    if (this->y_sensor_ != nullptr) {
      this->y_sensor_->publish_state(data.y);
    }
    if (this->z_sensor_ != nullptr) {
      this->z_sensor_->publish_state(data.z);
    }
    this->status_clear_warning();
  } else {
    ESP_LOGE(TAG, "failed to read data");
    this->status_set_warning();
  }
}

}  // namespace mlx90393
}  // namespace esphome
