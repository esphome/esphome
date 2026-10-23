#ifdef USE_ZEPHYR

#include "i2c_bus_zephyr.h"
#include <zephyr/drivers/i2c.h>
#include "esphome/core/log.h"

namespace esphome::i2c {

ESPHOME_LOG_TAG(TAG, "i2c");

static const char *get_speed(uint32_t dev_config) {
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
}

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
                "I2C Bus:\n"
                "  SDA Pin: GPIO%u\n"
                "  SCL Pin: GPIO%u\n"
                "  Frequency: %s\n"
                "  Name: %s",
                this->sda_pin_, this->scl_pin_, get_speed(this->dev_config_), this->i2c_dev_->name);

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

  i2c_msg msgs[2]{};
  size_t cnt = 0;
  uint8_t dst = 0x00;  // dummy data to not use random value

  if (read_count == 0 && write_count == 0) {
    msgs[cnt].buf = &dst;
    msgs[cnt].len = 0U;
    msgs[cnt++].flags = I2C_MSG_WRITE;
  } else {
    if (write_count) {
      // the same struct is used for read/write — const cast is fine; data isn't modified
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

static uint32_t speed_config(uint32_t dev_config, uint32_t frequency) {
  const uint32_t speed = frequency >= 400000 ? I2C_SPEED_FAST : I2C_SPEED_STANDARD;
  return (dev_config & ~I2C_SPEED_MASK) | I2C_SPEED_SET(speed);
}

void ZephyrI2CBus::set_frequency(uint32_t frequency) {
#ifdef I2C_PORT_FREQUENCY_COUNT
  this->frequency_ = frequency;
#endif
  this->dev_config_ = speed_config(this->dev_config_, frequency);
}

#ifdef I2C_PORT_FREQUENCY_COUNT
ErrorCode ZephyrI2CBus::switch_frequency(uint32_t frequency) {
  if (this->frequency_ == frequency) {
    return ERROR_OK;
  }
  const uint32_t dev_config = speed_config(this->dev_config_, frequency);
  // Until ready, setup() applies dev_config_
  if (this->is_ready() && i2c_configure(this->i2c_dev_, dev_config) < 0) {
    return ERROR_UNKNOWN;
  }
  this->dev_config_ = dev_config;
  this->frequency_ = frequency;
  return ERROR_OK;
}
#endif

}  // namespace esphome::i2c

#endif
