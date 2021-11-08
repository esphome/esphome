#include "sensor_mlx90393.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mlx90393 {

static const char *const TAG = "mlx90393";

void MLX90393_cls::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MLX90393_cls...");
  if(this->drdy_pin_== nullptr) {
    mlx.begin(0,0);
  } else {
    //mlx.begin(0,0, this->drdy_pin_);
    mlx.begin(0,0);
  }
  mlx.setGainSel(gain_);

  mlx.setResolution(resolutions_[0], resolutions_[1], resolutions_[2]);

  mlx.setOverSampling(osr_);

  mlx.setDigitalFiltering(filter_);
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
    if(this->x_sensor_!= nullptr) {
      this->x_sensor_->publish_state(data.x);
    }
    if(this->y_sensor_!= nullptr) {
      this->y_sensor_->publish_state(data.y);
    }
    if(this->z_sensor_!= nullptr) {
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
