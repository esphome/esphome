#ifdef USE_ZEPHYR

#include "i2c_bus_zephyr.h"
#include <zephyr/drivers/i2c.h>
#include "esphome/core/log.h"

namespace esphome::i2c {

static const char *const TAG = "i2c.zephyr";

void ZephyrI2CBus::setup() {
  if (!device_is_ready(this->i2c_dev_)) {
    ESP_LOGE(TAG, "I2C dev is not ready.");
    mark_failed();
    return;
  }

  int ret = i2c_configure(this->i2c_dev_, this->dev_config_);
  if (ret < 0) {
    ESP_LOGE(TAG, "I2C: Failed to configure device");
  }

  this->recovery_result_ = i2c_recover_bus(this->i2c_dev_);
  if (this->recovery_result_ != 0) {
    ESP_LOGE(TAG, "I2C recover bus failed, err %d", this->recovery_result_);
  }
  if (this->scan_) {
    ESP_LOGV(TAG, "Scanning I2C bus for active devices...");
    this->i2c_scan_();
  }
}

void ZephyrI2CBus::dump_config() {
  ESP_LOGCONFIG(TAG,
                "I2C Bus:""
                "  SDA Pin: GPIO%u\n"
                "  SCL Pin: GPIO%u\n",
				"  Name: %s",
                this->sda_pin_, this->scl_pin_, this->scl_pin_);
  ESP_LOGCONFIG(TAG, "  SDA Pin: GPIO%u", this->sda_pin_);
  ESP_LOGCONFIG(TAG, "  SCL Pin: GPIO%u", this->scl_pin_);
  ESP_LOGCONFIG(TAG, "  Name: %s", this->i2c_dev_->name);
  auto get_speed = [](uint32_t dev_config) {
    switch (I2C_SPEED_GET(dev_config)) {
      case I2C_SPEED_STANDARD:
        return "100 kHz";
      case I2C_SPEED_FAST:
        return "400 kHz";
      case I2C_SPEED_FAST_PLUS:
        return "1 MHz";
      case I2C_SPEED_HIGH:
        return "3.4 MHz";
      case I2C_SPEED_ULTRA:
        return "5 MHz";
    }
    return "unknown";
  };
  ESP_LOGCONFIG(TAG, "  Frequency: %s", get_speed(this->dev_config_));

  if (this->recovery_result_ != 0) {
    ESP_LOGCONFIG(TAG, "  Recovery: failed, err %d", this->recovery_result_);
  } else {
    ESP_LOGCONFIG(TAG, "  Recovery: bus successfully recovered");
  }
  if (this->scan_) {
    ESP_LOGI(TAG, "Results from I2C bus scan:");
    if (scan_results_.empty()) {
      ESP_LOGI(TAG, "Found no I2C devices!");
    } else {
      for (const auto &s : scan_results_) {
        if (s.second) {
          ESP_LOGI(TAG, "Found I2C device at address 0x%02X", s.first);
        } else {
          ESP_LOGE(TAG, "Unknown error at address 0x%02X", s.first);
        }
      }
    }
  }
}

ErrorCode ZephyrI2CBus::write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count,
                                    uint8_t *read_buffer, size_t read_count) {
  if (!device_is_ready(this->i2c_dev_)) {
    return ERROR_NOT_INITIALIZED;
  }

  i2c_msg msgs[8]{};
  size_t cnt = 0;
  uint8_t dst = 0x00;  // dummy data to not use random value

  if (read_count == 0 && write_count == 0) {
    msgs[cnt].buf = &dst;
    msgs[cnt].len = 0U;
    msgs[cnt++].flags = I2C_MSG_WRITE;
  } else {
    if (write_count) {
      msgs[cnt].buf = const_cast<uint8_t *>(write_buffer);
      msgs[cnt].len = write_count;
      msgs[cnt++].flags = I2C_MSG_WRITE;
    }
    if (read_count) {
      msgs[cnt].buf = const_cast<uint8_t *>(read_buffer);
      msgs[cnt].len = read_count;
      msgs[cnt++].flags = I2C_MSG_READ | I2C_MSG_RESTART;
    }
  }

  msgs[cnt - 1].flags |= I2C_MSG_STOP;

  auto err = i2c_transfer(this->i2c_dev_, msgs, cnt, address);

  if (err == -EIO) {
    return ERROR_NOT_ACKNOWLEDGED;
  }

  if (err != 0) {
    ESP_LOGE(TAG, "i2c transfer error %d", err);
    return ERROR_UNKNOWN;
  }

  return ERROR_OK;
}

void ZephyrI2CBus::set_frequency(uint32_t frequency) {
  this->dev_config_ &= ~I2C_SPEED_MASK;
  if (frequency == 400000) {
    this->dev_config_ |= I2C_SPEED_SET(I2C_SPEED_FAST);
  } else {
    this->dev_config_ |= I2C_SPEED_SET(I2C_SPEED_STANDARD);
  }
}

}  // namespace esphome::i2c

#endif
