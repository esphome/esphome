#ifdef USE_ESP_IDF

#include "i2c_bus_esp_idf.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <cstring>

namespace esphome {
namespace i2c {

static const char *const TAG = "i2c.idf";

void IDFI2CBus::setup() {
  static i2c_port_t next_port = 0;
  port_ = next_port++;

  recover_();

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
  if (this->scan_) {
    ESP_LOGV(TAG, "Scanning i2c bus for active devices...");
    this->i2c_scan_();
  }
}
void IDFI2CBus::dump_config() {
  ESP_LOGCONFIG(TAG, "I2C Bus:");
  ESP_LOGCONFIG(TAG, "  SDA Pin: GPIO%u", this->sda_pin_);
  ESP_LOGCONFIG(TAG, "  SCL Pin: GPIO%u", this->scl_pin_);
  ESP_LOGCONFIG(TAG, "  Frequency: %u Hz", this->frequency_);
  switch (this->recovery_result_) {
    case RECOVERY_COMPLETED:
      ESP_LOGCONFIG(TAG, "  Recovery: bus successfully recovered");
      break;
    case RECOVERY_FAILED_SCL_LOW:
      ESP_LOGCONFIG(TAG, "  Recovery: failed, SCL is held low on the bus");
      break;
    case RECOVERY_FAILED_SDA_LOW:
      ESP_LOGCONFIG(TAG, "  Recovery: failed, SDA is held low on the bus");
      break;
  }
  if (this->scan_) {
    ESP_LOGI(TAG, "Results from i2c bus scan:");
    if (scan_results_.empty()) {
      ESP_LOGI(TAG, "Found no i2c devices!");
    } else {
      for (const auto &s : scan_results_) {
        if (s.second) {
          ESP_LOGI(TAG, "Found i2c device at address 0x%02X", s.first);
        } else {
          ESP_LOGE(TAG, "Unknown error at address 0x%02X", s.first);
        }
      }
    }
  }
}

ErrorCode IDFI2CBus::readv(uint8_t address, ReadBuffer *buffers, size_t cnt) {
  // logging is only enabled with vv level, if warnings are shown the caller
  // should log them
  if (!initialized_) {
    ESP_LOGVV(TAG, "i2c bus not initialized!");
    return ERROR_NOT_INITIALIZED;
  }
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  esp_err_t err = i2c_master_start(cmd);
  if (err != ESP_OK) {
    ESP_LOGVV(TAG, "RX from %02X master start failed: %s", address, esp_err_to_name(err));
    i2c_cmd_link_delete(cmd);
    return ERROR_UNKNOWN;
  }
  err = i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_READ, true);
  if (err != ESP_OK) {
    ESP_LOGVV(TAG, "RX from %02X address write failed: %s", address, esp_err_to_name(err));
    i2c_cmd_link_delete(cmd);
    return ERROR_UNKNOWN;
  }
  for (size_t i = 0; i < cnt; i++) {
    const auto &buf = buffers[i];
    if (buf.len == 0)
      continue;
    err = i2c_master_read(cmd, buf.data, buf.len, i == cnt - 1 ? I2C_MASTER_LAST_NACK : I2C_MASTER_ACK);
    if (err != ESP_OK) {
      ESP_LOGVV(TAG, "RX from %02X data read failed: %s", address, esp_err_to_name(err));
      i2c_cmd_link_delete(cmd);
      return ERROR_UNKNOWN;
    }
  }
  err = i2c_master_stop(cmd);
  if (err != ESP_OK) {
    ESP_LOGVV(TAG, "RX from %02X stop failed: %s", address, esp_err_to_name(err));
    i2c_cmd_link_delete(cmd);
    return ERROR_UNKNOWN;
  }
  err = i2c_master_cmd_begin(port_, cmd, 20 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
  if (err == ESP_FAIL) {
    // transfer not acked
    ESP_LOGVV(TAG, "RX from %02X failed: not acked", address);
    return ERROR_NOT_ACKNOWLEDGED;
  } else if (err == ESP_ERR_TIMEOUT) {
    ESP_LOGVV(TAG, "RX from %02X failed: timeout", address);
    return ERROR_TIMEOUT;
  } else if (err != ESP_OK) {
    ESP_LOGVV(TAG, "RX from %02X failed: %s", address, esp_err_to_name(err));
    return ERROR_UNKNOWN;
  }

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
  char debug_buf[4];
  std::string debug_hex;

  for (size_t i = 0; i < cnt; i++) {
    const auto &buf = buffers[i];
    for (size_t j = 0; j < buf.len; j++) {
      snprintf(debug_buf, sizeof(debug_buf), "%02X", buf.data[j]);
      debug_hex += debug_buf;
    }
  }
  ESP_LOGVV(TAG, "0x%02X RX %s", address, debug_hex.c_str());
#endif

  return ERROR_OK;
}
ErrorCode IDFI2CBus::writev(uint8_t address, WriteBuffer *buffers, size_t cnt, bool stop) {
  // logging is only enabled with vv level, if warnings are shown the caller
  // should log them
  if (!initialized_) {
    ESP_LOGVV(TAG, "i2c bus not initialized!");
    return ERROR_NOT_INITIALIZED;
  }

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
  char debug_buf[4];
  std::string debug_hex;

  for (size_t i = 0; i < cnt; i++) {
    const auto &buf = buffers[i];
    for (size_t j = 0; j < buf.len; j++) {
      snprintf(debug_buf, sizeof(debug_buf), "%02X", buf.data[j]);
      debug_hex += debug_buf;
    }
  }
  ESP_LOGVV(TAG, "0x%02X TX %s", address, debug_hex.c_str());
#endif

  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  esp_err_t err = i2c_master_start(cmd);
  if (err != ESP_OK) {
    ESP_LOGVV(TAG, "TX to %02X master start failed: %s", address, esp_err_to_name(err));
    i2c_cmd_link_delete(cmd);
    return ERROR_UNKNOWN;
  }
  err = i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, true);
  if (err != ESP_OK) {
    ESP_LOGVV(TAG, "TX to %02X address write failed: %s", address, esp_err_to_name(err));
    i2c_cmd_link_delete(cmd);
    return ERROR_UNKNOWN;
  }
  for (size_t i = 0; i < cnt; i++) {
    const auto &buf = buffers[i];
    if (buf.len == 0)
      continue;
    err = i2c_master_write(cmd, buf.data, buf.len, true);
    if (err != ESP_OK) {
      ESP_LOGVV(TAG, "TX to %02X data write failed: %s", address, esp_err_to_name(err));
      i2c_cmd_link_delete(cmd);
      return ERROR_UNKNOWN;
    }
  }
  err = i2c_master_stop(cmd);
  if (err != ESP_OK) {
    ESP_LOGVV(TAG, "TX to %02X master stop failed: %s", address, esp_err_to_name(err));
    i2c_cmd_link_delete(cmd);
    return ERROR_UNKNOWN;
  }
  err = i2c_master_cmd_begin(port_, cmd, 20 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
  if (err == ESP_FAIL) {
    // transfer not acked
    ESP_LOGVV(TAG, "TX to %02X failed: not acked", address);
    return ERROR_NOT_ACKNOWLEDGED;
  } else if (err == ESP_ERR_TIMEOUT) {
    ESP_LOGVV(TAG, "TX to %02X failed: timeout", address);
    return ERROR_TIMEOUT;
  } else if (err != ESP_OK) {
    ESP_LOGVV(TAG, "TX to %02X failed: %s", address, esp_err_to_name(err));
    return ERROR_UNKNOWN;
  }
  return ERROR_OK;
}

/// Perform I2C bus recovery, see:
/// https://www.nxp.com/docs/en/user-guide/UM10204.pdf
/// https://www.analog.com/media/en/technical-documentation/application-notes/54305147357414AN686_0.pdf
void IDFI2CBus::recover_() {
  ESP_LOGI(TAG, "Performing I2C bus recovery");

  const gpio_num_t scl_pin = static_cast<gpio_num_t>(scl_pin_);
  const gpio_num_t sda_pin = static_cast<gpio_num_t>(sda_pin_);

  // For the upcoming operations, target for a 60kHz toggle frequency.
  // 1000kHz is the maximum frequency for I2C running in standard-mode,
  // but lower frequencies are not a problem.
  // Note: the timing that is used here is chosen manually, to get
  // results that are close to the timing that can be archieved by the
  // implementation for the Arduino framework.
  const auto half_period_usec = 7;

  // Configure SCL pin for open drain input/output, with a pull up resistor.
  gpio_set_level(scl_pin, 1);
  gpio_config_t scl_config{};
  scl_config.pin_bit_mask = 1ULL << scl_pin_;
  scl_config.mode = GPIO_MODE_INPUT_OUTPUT_OD;
  scl_config.pull_up_en = GPIO_PULLUP_ENABLE;
  scl_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
  scl_config.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&scl_config);

  // Configure SDA pin for open drain input/output, with a pull up resistor.
  gpio_set_level(sda_pin, 1);
  gpio_config_t sda_conf{};
  sda_conf.pin_bit_mask = 1ULL << sda_pin_;
  sda_conf.mode = GPIO_MODE_INPUT_OUTPUT_OD;
  sda_conf.pull_up_en = GPIO_PULLUP_ENABLE;
  sda_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  sda_conf.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&sda_conf);

  // If SCL is pulled low on the I2C bus, then some device is interfering
  // with the SCL line. In that case, the I2C bus cannot be recovered.
  delayMicroseconds(half_period_usec);
  if (gpio_get_level(scl_pin) == 0) {
    ESP_LOGE(TAG, "Recovery failed: SCL is held LOW on the I2C bus");
    recovery_result_ = RECOVERY_FAILED_SCL_LOW;
    return;
  }

  // From the specification:
  // "If the data line (SDA) is stuck LOW, send nine clock pulses. The
  //  device that held the bus LOW should release it sometime within
  //  those nine clocks."
  // We don't really have to detect if SDA is stuck low. We'll simply send
  // nine clock pulses here, just in case SDA is stuck. Actual checks on
  // the SDA line status will be done after the clock pulses.
  for (auto i = 0; i < 9; i++) {
    gpio_set_level(scl_pin, 0);
    delayMicroseconds(half_period_usec);
    gpio_set_level(scl_pin, 1);
    delayMicroseconds(half_period_usec);

    // When SCL is kept LOW at this point, we might be looking at a device
    // that applies clock stretching. Wait for the release of the SCL line,
    // but not forever. There is no specification for the maximum allowed
    // time. We'll stick to 500ms here.
    auto wait = 20;
    while (wait-- && gpio_get_level(scl_pin) == 0) {
      delay(25);
    }
    if (gpio_get_level(scl_pin) == 0) {
      ESP_LOGE(TAG, "Recovery failed: SCL is held LOW during clock pulse cycle");
      recovery_result_ = RECOVERY_FAILED_SCL_LOW;
      return;
    }
  }

  // By now, any stuck device ought to have sent all remaining bits of its
  // transaction, meaning that it should have freed up the SDA line, resulting
  // in SDA being pulled up.
  if (gpio_get_level(sda_pin) == 0) {
    ESP_LOGE(TAG, "Recovery failed: SDA is held LOW after clock pulse cycle");
    recovery_result_ = RECOVERY_FAILED_SDA_LOW;
    return;
  }

  // From the specification:
  // "I2C-bus compatible devices must reset their bus logic on receipt of
  //  a START or repeated START condition such that they all anticipate
  //  the sending of a target address, even if these START conditions are
  //  not positioned according to the proper format."
  // While the 9 clock pulses from above might have drained all bits of a
  // single byte within a transaction, a device might have more bytes to
  // transmit. So here we'll generate a START condition to snap the device
  // out of this state.
  // SCL and SDA are already high at this point, so we can generate a START
  // condition by making the SDA signal LOW.
  delayMicroseconds(half_period_usec);
  gpio_set_level(sda_pin, 0);

  // From the specification:
  // "A START condition immediately followed by a STOP condition (void
  //  message) is an illegal format. Many devices however are designed to
  //  operate properly under this condition."
  // Finally, we'll bring the I2C bus into a starting state by generating
  // a STOP condition.
  delayMicroseconds(half_period_usec);
  gpio_set_level(sda_pin, 1);

  recovery_result_ = RECOVERY_COMPLETED;
}

}  // namespace i2c
}  // namespace esphome

#endif  // USE_ESP_IDF
