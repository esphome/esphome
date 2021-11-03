#include "mlx90393.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mlx90393 {

static const char *const TAG = "mlx90393";

void MLX90393::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MLX90393...");
  if(!mlx.begin_I2C(this->address_)) {
    this->mark_failed();
    return;
  }
  mlx.setGain(gain_);

  mlx.setResolution(MLX90393_X, MLX90393_RES_19);
  mlx.setResolution(MLX90393_Y, MLX90393_RES_19);
  mlx.setResolution(MLX90393_Z, MLX90393_RES_16);

  mlx.setOversampling(MLX90393_OSR_2);

  mlx.setFilter(MLX90393_FILTER_6);
}

void MLX90393::dump_config() {
  ESP_LOGCONFIG(TAG, "MLX90393:");
  LOG_I2C_DEVICE(this);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with MLX90393 failed!");
    return;
  }
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "X Axis", this->x_sensor_);
  LOG_SENSOR("  ", "Y Axis", this->y_sensor_);
  LOG_SENSOR("  ", "Z Axis", this->z_sensor_);
}

float MLX90393::get_setup_priority() const { return setup_priority::DATA; }

void MLX90393::update() {
  float x,y,z=0;
  if (mlx.readData(&x, &y, &z)) {
    ESP_LOGD(TAG, "received %f %f %f", x, y, z);
    if(this->x_sensor_!= nullptr) {
      this->x_sensor_->publish_state(x);
    }
    if(this->y_sensor_!= nullptr) {
      this->y_sensor_->publish_state(y);
    }
    if(this->z_sensor_!= nullptr) {
      this->z_sensor_->publish_state(z);
    }
    this->status_clear_warning();
  } else {
    ESP_LOGE(TAG, "failed to read data");
    this->status_set_warning();
  }
}

}  // namespace mlx90393
}  // namespace esphome
