#ifdef USE_ZEPHYR

#include "i2c_bus_zephyr.h"
#include <zephyr/drivers/i2c.h>
#include "esphome/core/log.h"

namespace esphome {
namespace i2c {

static const char *const TAG = "i2c.zephyr";

void ZephyrI2CBus::setup() {
  const device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
  if (!device_is_ready(i2c_dev)) {
    ESP_LOGE(TAG, "I2C dev is not ready.");
    return;
  }
  i2c_dev_ = i2c_dev;

  this->recovery_result_ = i2c_recover_bus(i2c_dev_);
  if (this->recovery_result_ != 0) {
    ESP_LOGE(TAG, "I2C recover bus failed, err %d", this->recovery_result_);
  }
  if (this->scan_) {
    // FIXME it should be done one by one since it takes over 18sec
    ESP_LOGV(TAG, "Scanning I2C bus for active devices...");
    this->i2c_scan_();
  }
}

void ZephyrI2CBus::dump_config() {
  ESP_LOGCONFIG(TAG, "I2C Bus:");
  // ESP_LOGCONFIG(TAG, "  SDA Pin: GPIO%u", this->sda_pin_);
  // ESP_LOGCONFIG(TAG, "  SCL Pin: GPIO%u", this->scl_pin_);
  if (!i2c_dev_) {
    ESP_LOGCONFIG(TAG, "  Not initialized");
    return;
  }
  uint32_t dev_config = 0;
  // FIXME this function is not implemented in driver
  int err = i2c_get_config(i2c_dev_, &dev_config);
  if (err != 0) {
    ESP_LOGE(TAG, "Cannot get I2C config, err %d", err);
  } else {
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
    ESP_LOGCONFIG(TAG, "  Frequency: %s", get_speed(dev_config));
  }

  // if (timeout_ > 0) {
  //   ESP_LOGCONFIG(TAG, "  Timeout: %" PRIu32 "us", this->timeout_);
  // }
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

ErrorCode ZephyrI2CBus::readv(uint8_t address, ReadBuffer *buffers, size_t cnt) {
  if (!i2c_dev_) {
    return ERROR_NOT_INITIALIZED;
  }

  std::vector<i2c_msg> msgs(cnt);

  for (size_t i = 0; i < cnt; ++i) {
    msgs[i].buf = buffers[i].data;
    msgs[i].len = buffers[i].len;
    // TODO how to use I2C_MSG_RESTART
    msgs[i].flags = I2C_MSG_READ | I2C_MSG_RESTART;
  }
  msgs[cnt - 1].flags |= I2C_MSG_STOP;

  auto err = i2c_transfer(i2c_dev_, &msgs[0], msgs.size(), address);

  if (err == -EIO) {
    return ERROR_NOT_ACKNOWLEDGED;
  }

  if (err != 0) {
    ESP_LOGE(TAG, "i2c writing error %d", err);
    return ERROR_UNKNOWN;
  }

  return ERROR_OK;
}

ErrorCode ZephyrI2CBus::writev(uint8_t address, WriteBuffer *buffers, size_t cnt, bool stop) {
  if (!i2c_dev_) {
    return ERROR_NOT_INITIALIZED;
  }

  uint8_t dst = 0x00;  // dummy data to not use random value

  std::vector<i2c_msg> msgs(cnt == 0 ? 1 : cnt);

  for (size_t i = 0; i < cnt; ++i) {
    if (buffers) {
      msgs[i].buf = const_cast<uint8_t *>(buffers[i].data);
      msgs[i].len = buffers[i].len;
    } else {
      msgs[i].buf = &dst;
      msgs[i].len = 0U;
    }
    msgs[i].flags = I2C_MSG_WRITE;
  }

  if (stop) {
    msgs[cnt - 1].flags |= I2C_MSG_STOP;
  }

  auto err = i2c_transfer(i2c_dev_, &msgs[0], msgs.size(), address);

  if (err == -EIO) {
    return ERROR_NOT_ACKNOWLEDGED;
  }

  if (err != 0) {
    ESP_LOGE(TAG, "i2c writing error %d", err);
    return ERROR_UNKNOWN;
  }

  return ERROR_OK;
}

}  // namespace i2c
}  // namespace esphome

#endif  // USE_ESP_IDF
