#ifdef USE_ESP32

#include "i2c_bus_esp_idf.h"

#include <driver/gpio.h>
#include <cinttypes>
#include <cstring>
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace i2c {

static const char *const TAG = "i2c.idf";

void IDFI2CBus::setup() {
  static i2c_port_t next_hp_port = I2C_NUM_0;
#if SOC_LP_I2C_SUPPORTED
  static i2c_port_t next_lp_port = LP_I2C_NUM_0;
#endif

  if (this->timeout_ > 13000) {
    ESP_LOGW(TAG, "Using max allowed timeout: 13 ms");
    this->timeout_ = 13000;
  }

  this->recover_();

  i2c_master_bus_config_t bus_conf{};
  memset(&bus_conf, 0, sizeof(bus_conf));
  bus_conf.sda_io_num = gpio_num_t(sda_pin_);
  bus_conf.scl_io_num = gpio_num_t(scl_pin_);
  bus_conf.glitch_ignore_cnt = 7;
#if SOC_LP_I2C_SUPPORTED
  if (this->lp_mode_) {
    if ((next_lp_port - LP_I2C_NUM_0) == SOC_LP_I2C_NUM) {
      ESP_LOGE(TAG, "No more than %u LP buses supported", SOC_LP_I2C_NUM);
      this->mark_failed();
      return;
    }
    this->port_ = next_lp_port;
    next_lp_port = (i2c_port_t) (next_lp_port + 1);
    bus_conf.lp_source_clk = LP_I2C_SCLK_DEFAULT;
  } else {
#endif
    if (next_hp_port == SOC_HP_I2C_NUM) {
      ESP_LOGE(TAG, "No more than %u HP buses supported", SOC_HP_I2C_NUM);
      this->mark_failed();
      return;
    }
    this->port_ = next_hp_port;
    next_hp_port = (i2c_port_t) (next_hp_port + 1);
    bus_conf.clk_source = I2C_CLK_SRC_DEFAULT;
#if SOC_LP_I2C_SUPPORTED
  }
#endif
  bus_conf.i2c_port = this->port_;
  bus_conf.flags.enable_internal_pullup = sda_pullup_enabled_ || scl_pullup_enabled_;
  esp_err_t err = i2c_new_master_bus(&bus_conf, &this->bus_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  i2c_device_config_t dev_conf{};
  memset(&dev_conf, 0, sizeof(dev_conf));
  dev_conf.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  dev_conf.device_address = I2C_DEVICE_ADDRESS_NOT_USED;
  dev_conf.scl_speed_hz = this->frequency_;
  dev_conf.scl_wait_us = this->timeout_;
  err = i2c_master_bus_add_device(this->bus_, &dev_conf, &this->dev_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "i2c_master_bus_add_device failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  this->initialized_ = true;

  if (this->scan_) {
    ESP_LOGV(TAG, "Scanning for devices");
    this->i2c_scan_();
  }
}

void IDFI2CBus::dump_config() {
  ESP_LOGCONFIG(TAG, "I2C Bus:");
  ESP_LOGCONFIG(TAG,
                "  SDA Pin: GPIO%u\n"
                "  SCL Pin: GPIO%u\n"
                "  Frequency: %" PRIu32 " Hz",
                this->sda_pin_, this->scl_pin_, this->frequency_);
  if (timeout_ > 0) {
    ESP_LOGCONFIG(TAG, "  Timeout: %" PRIu32 "us", this->timeout_);
  }
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
    ESP_LOGCONFIG(TAG, "Results from bus scan:");
    if (scan_results_.empty()) {
      ESP_LOGCONFIG(TAG, "Found no devices");
    } else {
      for (const auto &s : scan_results_) {
        if (s.second) {
          ESP_LOGCONFIG(TAG, "Found device at address 0x%02X", s.first);
        } else {
          ESP_LOGE(TAG, "Unknown error at address 0x%02X", s.first);
        }
      }
    }
  }
}

ErrorCode IDFI2CBus::write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                                 size_t read_count) {
  // logging is only enabled with v level, if warnings are shown the caller
  // should log them
  if (!initialized_) {
    ESP_LOGW(TAG, "i2c bus not initialized!");
    return ERROR_NOT_INITIALIZED;
  }

  i2c_operation_job_t jobs[8]{};
  size_t num_jobs = 0;
  uint8_t write_addr = (address << 1) | I2C_MASTER_WRITE;
  uint8_t read_addr = (address << 1) | I2C_MASTER_READ;
  ESP_LOGV(TAG, "Writing %zu bytes, reading %zu bytes", write_count, read_count);
  if (read_count == 0 && write_count == 0) {
    // basically just a bus probe. Send a start, address and stop
    ESP_LOGV(TAG, "0x%02X BUS PROBE", address);
    jobs[num_jobs++].command = I2C_MASTER_CMD_START;
    jobs[num_jobs].command = I2C_MASTER_CMD_WRITE;
    jobs[num_jobs].write.ack_check = true;
    jobs[num_jobs].write.data = &write_addr;
    jobs[num_jobs++].write.total_bytes = 1;
  } else {
    if (write_count != 0) {
      ESP_LOGV(TAG, "0x%02X TX %s", address, format_hex_pretty(write_buffer, write_count).c_str());
      jobs[num_jobs++].command = I2C_MASTER_CMD_START;
      jobs[num_jobs].command = I2C_MASTER_CMD_WRITE;
      jobs[num_jobs].write.ack_check = true;
      jobs[num_jobs].write.data = &write_addr;
      jobs[num_jobs++].write.total_bytes = 1;
      jobs[num_jobs].command = I2C_MASTER_CMD_WRITE;
      jobs[num_jobs].write.ack_check = true;
      jobs[num_jobs].write.data = (uint8_t *) write_buffer;
      jobs[num_jobs++].write.total_bytes = write_count;
    }
    if (read_count != 0) {
      ESP_LOGV(TAG, "0x%02X RX bytes %zu", address, read_count);
      jobs[num_jobs++].command = I2C_MASTER_CMD_START;
      jobs[num_jobs].command = I2C_MASTER_CMD_WRITE;
      jobs[num_jobs].write.ack_check = true;
      jobs[num_jobs].write.data = &read_addr;
      jobs[num_jobs++].write.total_bytes = 1;
      if (read_count > 1) {
        jobs[num_jobs].command = I2C_MASTER_CMD_READ;
        jobs[num_jobs].read.ack_value = I2C_ACK_VAL;
        jobs[num_jobs].read.data = read_buffer;
        jobs[num_jobs++].read.total_bytes = read_count - 1;
      }
      jobs[num_jobs].command = I2C_MASTER_CMD_READ;
      jobs[num_jobs].read.ack_value = I2C_NACK_VAL;
      jobs[num_jobs].read.data = read_buffer + read_count - 1;
      jobs[num_jobs++].read.total_bytes = 1;
    }
  }
  jobs[num_jobs++].command = I2C_MASTER_CMD_STOP;
  ESP_LOGV(TAG, "Sending %zu jobs", num_jobs);
  esp_err_t err = i2c_master_execute_defined_operations(this->dev_, jobs, num_jobs, 20);
  if (err == ESP_ERR_INVALID_STATE) {
    ESP_LOGV(TAG, "TX to %02X failed: not acked", address);
    return ERROR_NOT_ACKNOWLEDGED;
  } else if (err == ESP_ERR_TIMEOUT) {
    ESP_LOGV(TAG, "TX to %02X failed: timeout", address);
    return ERROR_TIMEOUT;
  } else if (err != ESP_OK) {
    ESP_LOGV(TAG, "TX to %02X failed: %s", address, esp_err_to_name(err));
    return ERROR_UNKNOWN;
  }
  return ERROR_OK;
}

/// Perform I2C bus recovery, see:
/// https://www.nxp.com/docs/en/user-guide/UM10204.pdf
/// https://www.analog.com/media/en/technical-documentation/application-notes/54305147357414AN686_0.pdf
void IDFI2CBus::recover_() {
  ESP_LOGI(TAG, "Performing bus recovery");

  const auto scl_pin = static_cast<gpio_num_t>(scl_pin_);
  const auto sda_pin = static_cast<gpio_num_t>(sda_pin_);

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
    ESP_LOGE(TAG, "Recovery failed: SCL is held LOW on the bus");
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
    // time. We yield and reset the WDT, so as to avoid triggering reset.
    // No point in trying to recover the bus by forcing a uC reset. Bus
    // should recover in a few ms or less else not likely to recovery at
    // all.
    auto wait = 250;
    while (wait-- && gpio_get_level(scl_pin) == 0) {
      App.feed_wdt();
      delayMicroseconds(half_period_usec * 2);
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
#endif  // USE_ESP32
