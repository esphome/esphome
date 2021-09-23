#ifdef USE_ESP_IDF

#include "i2c_bus_esp_idf.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace i2c {

static const char *const TAG = "i2c.idf";

void IDFI2CBus::setup() {
  static i2c_port_t next_port = 0;
  port_ = next_port++;

  recover();

  i2c_config_t conf{};
  memset(&conf, 0, sizeof(conf));
  conf.mode = I2C_MODE_MASTER;
  conf.sda_io_num = sda_pin_;
  conf.sda_pullup_en = sda_pullup_enabled_;
  conf.scl_io_num = scl_pin_;
  conf.scl_pullup_en = scl_pullup_enabled_;
  conf.master.clk_speed = frequency_;
  esp_err_t err = i2c_param_config(port_, &conf);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "i2c_param_config failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }
  err = i2c_driver_install(port_, I2C_MODE_MASTER, 0, 0, ESP_INTR_FLAG_IRAM);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "i2c_driver_install failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }
  initialized_ = true;
}
void IDFI2CBus::dump_config() {
  ESP_LOGCONFIG(TAG, "I2C Bus:");
  ESP_LOGCONFIG(TAG, "  SDA Pin: GPIO%u", this->sda_pin_);
  ESP_LOGCONFIG(TAG, "  SCL Pin: GPIO%u", this->scl_pin_);
  ESP_LOGCONFIG(TAG, "  Frequency: %u Hz", this->frequency_);
  if (this->scan_) {
    ESP_LOGI(TAG, "Scanning i2c bus for active devices...");
    uint8_t found = 0;
    for (uint8_t address = 8; address < 120; address++) {
      auto err = writev(address, nullptr, 0);

      if (err == ERROR_OK) {
        ESP_LOGI(TAG, "Found i2c device at address 0x%02X", address);
        found++;
      } else if (err == ERROR_UNKNOWN) {
        ESP_LOGI(TAG, "Unknown error at address 0x%02X", address);
      }
    }
    if (found == 0) {
      ESP_LOGI(TAG, "Found no i2c devices!");
    }
  }
}
ErrorCode IDFI2CBus::readv(uint8_t address, ReadBuffer *buffers, size_t cnt) {
  if (!initialized_)
    return ERROR_NOT_INITIALIZED;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  esp_err_t err = i2c_master_start(cmd);
  if (err != ESP_OK) {
    i2c_cmd_link_delete(cmd);
    return ERROR_UNKNOWN;
  }
  err = i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_READ, true);
  if (err != ESP_OK) {
    i2c_cmd_link_delete(cmd);
    return ERROR_UNKNOWN;
  }
  for (size_t i = 0; i < cnt; i++) {
    const auto &buf = buffers[i];
    if (buf.len == 0)
      continue;
    err = i2c_master_read(cmd, buf.data, buf.len, i == cnt - 1 ? I2C_MASTER_LAST_NACK : I2C_MASTER_ACK);
    if (err != ESP_OK) {
      i2c_cmd_link_delete(cmd);
      return ERROR_UNKNOWN;
    }
  }
  err = i2c_master_stop(cmd);
  if (err != ESP_OK) {
    i2c_cmd_link_delete(cmd);
    return ERROR_UNKNOWN;
  }
  err = i2c_master_cmd_begin(port_, cmd, 20 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
  if (err == ESP_FAIL) {
    // transfer not acked
    return ERROR_NOT_ACKNOWLEDGED;
  } else if (err == ESP_ERR_TIMEOUT) {
    return ERROR_TIMEOUT;
  } else if (err != ESP_OK) {
    return ERROR_UNKNOWN;
  }
  return ERROR_OK;
}
ErrorCode IDFI2CBus::writev(uint8_t address, WriteBuffer *buffers, size_t cnt) {
  if (!initialized_)
    return ERROR_NOT_INITIALIZED;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  esp_err_t err = i2c_master_start(cmd);
  if (err != ESP_OK) {
    i2c_cmd_link_delete(cmd);
    return ERROR_UNKNOWN;
  }
  err = i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, true);
  if (err != ESP_OK) {
    i2c_cmd_link_delete(cmd);
    return ERROR_UNKNOWN;
  }
  for (size_t i = 0; i < cnt; i++) {
    const auto &buf = buffers[i];
    if (buf.len == 0)
      continue;
    err = i2c_master_write(cmd, buf.data, buf.len, true);
    if (err != ESP_OK) {
      i2c_cmd_link_delete(cmd);
      return ERROR_UNKNOWN;
    }
  }
  err = i2c_master_stop(cmd);
  if (err != ESP_OK) {
    i2c_cmd_link_delete(cmd);
    return ERROR_UNKNOWN;
  }
  err = i2c_master_cmd_begin(port_, cmd, 20 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
  if (err == ESP_FAIL) {
    // transfer not acked
    return ERROR_NOT_ACKNOWLEDGED;
  } else if (err == ESP_ERR_TIMEOUT) {
    return ERROR_TIMEOUT;
  } else if (err != ESP_OK) {
    return ERROR_UNKNOWN;
  }
  return ERROR_OK;
}

void IDFI2CBus::recover() {
  // Perform I2C bus recovery, see
  // https://www.analog.com/media/en/technical-documentation/application-notes/54305147357414AN686_0.pdf
  // or see the linux kernel implementation, e.g.
  // https://elixir.bootlin.com/linux/v5.14.6/source/drivers/i2c/i2c-core-base.c#L200

  // try to get about 100kHz toggle frequency
  const auto half_period_usec = 1000000 / 100000 / 2;
  const auto recover_scl_periods = 9;
  const gpio_num_t scl_pin = static_cast<gpio_num_t>(scl_pin_);

  // configure scl as output
  gpio_config_t conf{};
  conf.pin_bit_mask = 1ULL << static_cast<uint32_t>(scl_pin_);
  conf.mode = GPIO_MODE_OUTPUT;
  conf.pull_up_en = GPIO_PULLUP_DISABLE;
  conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  conf.intr_type = GPIO_INTR_DISABLE;

  gpio_config(&conf);

  // set scl high
  gpio_set_level(scl_pin, 1);

  // in total generate 9 falling-rising edges
  for (auto i = 0; i < recover_scl_periods; i++) {
    delayMicroseconds(half_period_usec);
    gpio_set_level(scl_pin, 0);
    delayMicroseconds(half_period_usec);
    gpio_set_level(scl_pin, 1);
  }

  delayMicroseconds(half_period_usec);
}

}  // namespace i2c
}  // namespace esphome

#endif  // USE_ESP_IDF
