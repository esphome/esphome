#ifdef USE_ESP32

#include "i2c_bus_esp_idf.h"

#include <driver/gpio.h>
#include <cinttypes>
#include <cstring>
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_I2C_ISR_GUARD
#include <esp_intr_alloc.h>
#include <hal/i2c_ll.h>  // declares i2c_periph_signal (IDF 5: soc/, IDF 6: hal/)
#if SOC_I2C_SUPPORT_HW_FSM_RST || I2C_LL_SUPPORT_HW_FSM_RST
#error "This chip has an I2C FSM reset; the ISR guard is no longer needed"
#endif

namespace esphome::i2c {
// Without an FSM reset ACK_ERR can stay raised and the IDF ISR never masks it (esp-idf#17690). Mask the master
// events until the next transaction when an error is raised both before and after the handler clears it.
static constexpr uint32_t I2C_ERROR_INTR = I2C_LL_INTR_NACK | I2C_LL_INTR_TIMEOUT | I2C_LL_INTR_ARBITRATION;
static_assert((I2C_LL_MASTER_EVENT_INTR & I2C_ERROR_INTR) == I2C_ERROR_INTR,
              "The master event mask must cover the error interrupts");

struct I2CIsrGuard {
  intr_handler_t handler;
  void *arg;
  i2c_dev_t *dev;
};
static I2CIsrGuard i2c_isr_guards[SOC_HP_I2C_NUM];  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static void IRAM_ATTR i2c_isr_guard(void *arg) {
  auto *guard = static_cast<I2CIsrGuard *>(arg);
  uint32_t before;
  i2c_ll_get_intr_mask(guard->dev, &before);
  guard->handler(guard->arg);
  uint32_t after;
  i2c_ll_get_intr_mask(guard->dev, &after);
  if (before & after & I2C_ERROR_INTR)
    i2c_ll_disable_intr_mask(guard->dev, I2C_LL_MASTER_EVENT_INTR);
}

static int i2c_port_for_source(int source) {
  for (int port = 0; port < SOC_HP_I2C_NUM; port++) {
    if (source == i2c_periph_signal[port].irq)
      return port;
  }
  return -1;
}
}  // namespace esphome::i2c

// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" {
esp_err_t __real_esp_intr_alloc_intrstatus(int source, int flags, uint32_t reg, uint32_t mask, intr_handler_t handler,
                                           void *arg, intr_handle_t *ret_handle);

esp_err_t __wrap_esp_intr_alloc_intrstatus(int source, int flags, uint32_t reg, uint32_t mask, intr_handler_t handler,
                                           void *arg, intr_handle_t *ret_handle) {
  int port = esphome::i2c::i2c_port_for_source(source);
  if (port < 0)
    return __real_esp_intr_alloc_intrstatus(source, flags, reg, mask, handler, arg, ret_handle);
  auto &guard = esphome::i2c::i2c_isr_guards[port];
  auto previous = guard;
  guard = {handler, arg, I2C_LL_GET_HW(port)};
  esp_err_t err =
      __real_esp_intr_alloc_intrstatus(source, flags, reg, mask, esphome::i2c::i2c_isr_guard, &guard, ret_handle);
  if (err != ESP_OK)
    guard = previous;  // a failed allocation leaves the live guard alone
  return err;
}
}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
#endif  // USE_I2C_ISR_GUARD

namespace esphome::i2c {

ESPHOME_LOG_TAG(TAG, "i2c");

// Maximum bytes to log in hex format (truncates larger transfers)
static constexpr size_t I2C_MAX_LOG_BYTES = 32;

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
  bus_conf.sda_io_num = gpio_num_t(this->sda_pin_);
  bus_conf.scl_io_num = gpio_num_t(this->scl_pin_);
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
  bus_conf.flags.enable_internal_pullup = this->sda_pullup_enabled_ || this->scl_pullup_enabled_;
  esp_err_t err = i2c_new_master_bus(&bus_conf, &this->bus_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  this->dev_ = this->add_device_(this->frequency_);
  if (this->dev_ == nullptr) {
    this->mark_failed();
    return;
  }
#ifdef I2C_PORT_FREQUENCY_COUNT
  this->add_frequency(this->frequency_);
  for (auto &device : this->devices_) {
    device.dev = device.frequency == this->frequency_ ? this->dev_ : this->add_device_(device.frequency);
    if (device.dev == nullptr) {
      this->mark_failed();
      return;
    }
  }
#endif

  this->initialized_ = true;

  if (this->scan_) {
    ESP_LOGV(TAG, "Scanning for devices");
    this->i2c_scan_();
  }
}

i2c_master_dev_handle_t IDFI2CBus::add_device_(uint32_t frequency) {
  i2c_device_config_t dev_conf{};
  dev_conf.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  dev_conf.device_address = I2C_DEVICE_ADDRESS_NOT_USED;
  dev_conf.scl_speed_hz = frequency;
  dev_conf.scl_wait_us = this->timeout_;
  i2c_master_dev_handle_t dev{};
  const esp_err_t err = i2c_master_bus_add_device(this->bus_, &dev_conf, &dev);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "i2c_master_bus_add_device failed: %s", esp_err_to_name(err));
    return nullptr;
  }
  return dev;
}

void IDFI2CBus::dump_config() {
  ESP_LOGCONFIG(TAG, "I2C Bus:");
  ESP_LOGCONFIG(TAG,
                "  SDA Pin: GPIO%u\n"
                "  SCL Pin: GPIO%u\n"
                "  Frequency: %" PRIu32 " Hz",
                this->sda_pin_, this->scl_pin_, this->frequency_);
  if (this->timeout_ > 0) {
    ESP_LOGCONFIG(TAG, "  Timeout: %" PRIu32 "us", this->timeout_);
  }
#ifdef USE_I2C_ISR_GUARD
  // Null when the IDF driver no longer allocates its interrupt through the wrapped call
  if (i2c_isr_guards[this->port_].handler == nullptr) {
    ESP_LOGW(TAG, "  Interrupt guard inactive");
  }
#endif
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
  this->dump_scan_results_();
}

ErrorCode IDFI2CBus::write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                                 size_t read_count) {
  // logging is only enabled with v level, if warnings are shown the caller
  // should log them
  if (!this->initialized_) {
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
      ESP_LOGV_HEX(TAG, write_buffer, write_count, I2C_MAX_LOG_BYTES, "0x%02X TX %s", address);
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
  esp_err_t err = i2c_master_execute_defined_operations(this->dev_, jobs, num_jobs, 100);
  if (err == ESP_ERR_INVALID_STATE || err == ESP_ERR_INVALID_RESPONSE) {
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

#ifdef I2C_PORT_FREQUENCY_COUNT
void IDFI2CBus::add_frequency(uint32_t frequency) {
  for (const auto &device : this->devices_) {
    if (device.frequency == frequency) {
      return;
    }
  }
  this->devices_.push_back({frequency, nullptr});
}

ErrorCode IDFI2CBus::switch_frequency(uint32_t frequency) {
  if (this->frequency_ == frequency) {
    return ERROR_OK;
  }
  if (!this->initialized_) {
    this->frequency_ = frequency;
    return ERROR_OK;
  }
  // Every handle exists since setup(); a frequency nobody registered has none
  for (const auto &device : this->devices_) {
    if (device.frequency == frequency) {
      this->dev_ = device.dev;
      this->frequency_ = frequency;
      return ERROR_OK;
    }
  }
  return ERROR_UNKNOWN;
}
#endif

/// Perform I2C bus recovery, see:
/// https://www.nxp.com/docs/en/user-guide/UM10204.pdf
/// https://www.analog.com/media/en/technical-documentation/application-notes/54305147357414AN686_0.pdf
void IDFI2CBus::recover_() {
  ESP_LOGI(TAG, "Performing bus recovery");

  const auto scl_pin = static_cast<gpio_num_t>(this->scl_pin_);
  const auto sda_pin = static_cast<gpio_num_t>(this->sda_pin_);

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
  scl_config.pin_bit_mask = 1ULL << this->scl_pin_;
  scl_config.mode = GPIO_MODE_INPUT_OUTPUT_OD;
  scl_config.pull_up_en = GPIO_PULLUP_ENABLE;
  scl_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
  scl_config.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&scl_config);

  // Configure SDA pin for open drain input/output, with a pull up resistor.
  gpio_set_level(sda_pin, 1);
  gpio_config_t sda_conf{};
  sda_conf.pin_bit_mask = 1ULL << this->sda_pin_;
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
    this->recovery_result_ = RECOVERY_FAILED_SCL_LOW;
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
      this->recovery_result_ = RECOVERY_FAILED_SCL_LOW;
      return;
    }
  }

  // By now, any stuck device ought to have sent all remaining bits of its
  // transaction, meaning that it should have freed up the SDA line, resulting
  // in SDA being pulled up.
  if (gpio_get_level(sda_pin) == 0) {
    ESP_LOGE(TAG, "Recovery failed: SDA is held LOW after clock pulse cycle");
    this->recovery_result_ = RECOVERY_FAILED_SDA_LOW;
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

  this->recovery_result_ = RECOVERY_COMPLETED;
}

}  // namespace esphome::i2c
#endif  // USE_ESP32
