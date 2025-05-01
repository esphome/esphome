#include "sensor_mlx90393.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mlx90393 {

static const char *const TAG = "mlx90393";

const LogString *settings_to_string(MLX90393Setting setting) {
  switch (setting) {
    case MLX90393_GAIN_SEL:
      return LOG_STR("gain");
    case MLX90393_RESOLUTION:
      return LOG_STR("resolution");
    case MLX90393_OVER_SAMPLING:
      return LOG_STR("oversampling");
    case MLX90393_DIGITAL_FILTERING:
      return LOG_STR("digital filtering");
    case MLX90393_TEMPERATURE_OVER_SAMPLING:
      return LOG_STR("temperature oversampling");
    case MLX90393_TEMPERATURE_COMPENSATION:
      return LOG_STR("temperature compensation");
    case MLX90393_HALLCONF:
      return LOG_STR("hallconf");
    case MLX90393_LAST:
      return LOG_STR("error");
    default:
      return LOG_STR("unknown");
  }
};

bool MLX90393Cls::transceive(const uint8_t *request, size_t request_size, uint8_t *response, size_t response_size) {
  i2c::ErrorCode e = this->write(request, request_size);
  if (e != i2c::ErrorCode::ERROR_OK) {
    ESP_LOGV(TAG, "i2c failed to write %u", e);
    return false;
  }
  e = this->read(response, response_size);
  if (e != i2c::ErrorCode::ERROR_OK) {
    ESP_LOGV(TAG, "i2c failed to read %u", e);
    return false;
  }
  return true;
}

bool MLX90393Cls::has_drdy_pin() { return this->drdy_pin_ != nullptr; }

bool MLX90393Cls::read_drdy_pin() {
  if (this->drdy_pin_ == nullptr) {
    return false;
  } else {
    return this->drdy_pin_->digital_read();
  }
}
void MLX90393Cls::sleep_millis(uint32_t millis) { delay(millis); }
void MLX90393Cls::sleep_micros(uint32_t micros) { delayMicroseconds(micros); }

uint8_t MLX90393Cls::apply_setting_(MLX90393Setting which) {
  uint8_t ret = -1;
  switch (which) {
    case MLX90393_GAIN_SEL:
      ret = this->mlx_.setGainSel(this->gain_);
      break;
    case MLX90393_RESOLUTION:
      ret = this->mlx_.setResolution(this->resolutions_[0], this->resolutions_[1], this->resolutions_[2]);
      break;
    case MLX90393_OVER_SAMPLING:
      ret = this->mlx_.setOverSampling(this->oversampling_);
      break;
    case MLX90393_DIGITAL_FILTERING:
      ret = this->mlx_.setDigitalFiltering(this->filter_);
      break;
    case MLX90393_TEMPERATURE_OVER_SAMPLING:
      ret = this->mlx_.setTemperatureOverSampling(this->temperature_oversampling_);
      break;
    case MLX90393_TEMPERATURE_COMPENSATION:
      ret = this->mlx_.setTemperatureCompensation(this->temperature_compensation_);
      break;
    case MLX90393_HALLCONF:
      ret = this->mlx_.setHallConf(this->hallconf_);
      break;
    default:
      break;
  }
  if (ret != MLX90393::STATUS_OK) {
    ESP_LOGE(TAG, "failed to apply %s", LOG_STR_ARG(settings_to_string(which)));
  }
  return ret;
}

bool MLX90393Cls::apply_all_settings_() {
  // perform dummy read after reset
  // first one always gets NAK even tough everything is fine
  uint8_t ignore = 0;
  this->mlx_.getGainSel(ignore);

  uint8_t result = MLX90393::STATUS_OK;
  for (int i = MLX90393_GAIN_SEL; i != MLX90393_LAST; i++) {
    MLX90393Setting stage = static_cast<MLX90393Setting>(i);
    result |= this->apply_setting_(stage);
  }
  return result == MLX90393::STATUS_OK;
}

void MLX90393Cls::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MLX90393...");
  // note the two arguments A0 and A1 which are used to construct an i2c address
  // we can hard-code these because we never actually use the constructed address
  // see the transceive function above, which uses the address from I2CComponent
  this->mlx_.begin_with_hal(this, 0, 0);

  if (!this->apply_all_settings_()) {
    this->mark_failed();
  }

  // start verify settings process
  this->set_timeout("verify settings", 3000, [this]() { this->verify_settings_timeout_(MLX90393_GAIN_SEL); });
}

void MLX90393Cls::dump_config() {
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
  LOG_SENSOR("  ", "Temperature", this->t_sensor_);
}

float MLX90393Cls::get_setup_priority() const { return setup_priority::DATA; }

void MLX90393Cls::update() {
  MLX90393::txyz data;

  if (this->mlx_.readData(data) == MLX90393::STATUS_OK) {
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
    if (this->t_sensor_ != nullptr) {
      this->t_sensor_->publish_state(data.t);
    }
    this->status_clear_warning();
  } else {
    ESP_LOGE(TAG, "failed to read data");
    this->status_set_warning();
  }
}

bool MLX90393Cls::verify_setting_(MLX90393Setting which) {
  uint8_t read_value = 0xFF;
  uint8_t expected_value = 0xFF;
  uint8_t read_status = -1;
  char read_back_str[25] = {0};

  switch (which) {
    case MLX90393_GAIN_SEL: {
      read_status = this->mlx_.getGainSel(read_value);
      expected_value = this->gain_;
      break;
    }

    case MLX90393_RESOLUTION: {
      uint8_t read_resolutions[3] = {0xFF};
      read_status = this->mlx_.getResolution(read_resolutions[0], read_resolutions[1], read_resolutions[2]);
      snprintf(read_back_str, sizeof(read_back_str), "%u %u %u expected %u %u %u", read_resolutions[0],
               read_resolutions[1], read_resolutions[2], this->resolutions_[0], this->resolutions_[1],
               this->resolutions_[2]);
      bool is_correct = true;
      for (int i = 0; i < 3; i++) {
        is_correct &= read_resolutions[i] == this->resolutions_[i];
      }
      if (is_correct) {
        // set read_value and expected_value to same number, so the code blow recognizes it is correct
        read_value = 0;
        expected_value = 0;
      } else {
        // set to different numbers, to show incorrect
        read_value = 1;
        expected_value = 0;
      }
      break;
    }
    case MLX90393_OVER_SAMPLING: {
      read_status = this->mlx_.getOverSampling(read_value);
      expected_value = this->oversampling_;
      break;
    }
    case MLX90393_DIGITAL_FILTERING: {
      read_status = this->mlx_.getDigitalFiltering(read_value);
      expected_value = this->filter_;
      break;
    }
    case MLX90393_TEMPERATURE_OVER_SAMPLING: {
      read_status = this->mlx_.getTemperatureOverSampling(read_value);
      expected_value = this->temperature_oversampling_;
      break;
    }
    case MLX90393_TEMPERATURE_COMPENSATION: {
      read_status = this->mlx_.getTemperatureCompensation(read_value);
      expected_value = (bool) this->temperature_compensation_;
      break;
    }
    case MLX90393_HALLCONF: {
      read_status = this->mlx_.getHallConf(read_value);
      expected_value = this->hallconf_;
      break;
    }
    default: {
      return false;
    }
  }
  if (read_status != MLX90393::STATUS_OK) {
    ESP_LOGE(TAG, "verify error: failed to read %s", LOG_STR_ARG(settings_to_string(which)));
    return false;
  }
  if (read_back_str[0] == 0x0) {
    snprintf(read_back_str, sizeof(read_back_str), "%u expected %u", read_value, expected_value);
  }
  bool is_correct = read_value == expected_value;
  if (!is_correct) {
    ESP_LOGW(TAG, "verify failed: read back wrong %s: got %s", LOG_STR_ARG(settings_to_string(which)), read_back_str);
    return false;
  }
  ESP_LOGD(TAG, "verify succeeded for %s. got %s", LOG_STR_ARG(settings_to_string(which)), read_back_str);
  return true;
}

/**
 * Regularly checks that our settings are still applied.
 * Used to catch spurious chip resets.
 *
 * returns true if everything is fine.
 * false if not
 */
void MLX90393Cls::verify_settings_timeout_(MLX90393Setting stage) {
  bool is_setting_ok = this->verify_setting_(stage);

  if (!is_setting_ok) {
    if (this->mlx_.checkStatus(this->mlx_.reset()) != MLX90393::STATUS_OK) {
      ESP_LOGE(TAG, "failed to reset device");
      this->status_set_error();
      this->mark_failed();
      return;
    }

    if (!this->apply_all_settings_()) {
      ESP_LOGE(TAG, "failed to re-apply settings");
      this->status_set_error();
      this->mark_failed();
    } else {
      ESP_LOGI(TAG, "reset and re-apply settings completed");
    }
  }

  MLX90393Setting next_stage = static_cast<MLX90393Setting>(static_cast<int>(stage) + 1);
  if (next_stage == MLX90393_LAST) {
    next_stage = static_cast<MLX90393Setting>(0);
  }

  this->set_timeout("verify settings", 3000, [this, next_stage]() { this->verify_settings_timeout_(next_stage); });
}

}  // namespace mlx90393
}  // namespace esphome
