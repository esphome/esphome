#include "m5ioe1.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/log.h"

namespace esphome::m5ioe1 {

static const char *TAG = "m5ioe1";
// GPIO Registers
static constexpr uint8_t REG_UID_L = 0x00;       // UID
static constexpr uint8_t REG_UID_H = 0x01;       // UID
static constexpr uint8_t REG_VERSION = 0x02;     // version
static constexpr uint8_t REG_GPIO_M_L = 0x03;    // GPIO Mode Low
static constexpr uint8_t REG_GPIO_M_H = 0x04;    // GPIO Mode High
static constexpr uint8_t REG_GPIO_O_L = 0x05;    // GPIO Output Low
static constexpr uint8_t REG_GPIO_O_H = 0x06;    // GPIO Output High
static constexpr uint8_t REG_GPIO_I_L = 0x07;    // GPIO Input Low
static constexpr uint8_t REG_GPIO_I_H = 0x08;    // GPIO Input High
static constexpr uint8_t REG_GPIO_PU_L = 0x09;   // GPIO Pull-up Low
static constexpr uint8_t REG_GPIO_IP_L = 0x0F;   // GPIO interrupt type Low
static constexpr uint8_t REG_GPIO_IP_H = 0x10;   // GPIO interrupt type High
static constexpr uint8_t REG_GPIO_PU_H = 0x0A;   // GPIO Pull-up High
static constexpr uint8_t REG_GPIO_PD_L = 0x0B;   // GPIO Pull-down Low
static constexpr uint8_t REG_GPIO_PD_H = 0x0C;   // GPIO Pull-down High
static constexpr uint8_t REG_GPIO_IE_L = 0x0D;   // GPIO Input Enable Low
static constexpr uint8_t REG_GPIO_IE_H = 0x0E;   // GPIO Input Enable High
static constexpr uint8_t REG_GPIO_IS_L = 0x11;   // GPIO Input Source Low
static constexpr uint8_t REG_GPIO_IS_H = 0x12;   // GPIO Input Source High
static constexpr uint8_t REG_GPIO_DRV_L = 0x13;  // GPIO Drive Low
static constexpr uint8_t REG_GPIO_DRV_H = 0x14;  // GPIO Drive High

// ADC Registers
static constexpr uint8_t REG_ADC_CTRL = 0x15;
static constexpr uint8_t REG_ADC_D_L = 0x16;
static constexpr uint8_t REG_ADC_D_H = 0x17;

// Temperature Sensor Registers
static constexpr uint8_t REG_TEMP_CTRL = 0x18;
static constexpr uint8_t REG_TEMP_D_L = 0x19;
static constexpr uint8_t REG_TEMP_D_H = 0x1A;

// PWM Duty Registers
static constexpr uint8_t REG_PWM1_DUTY_L = 0x1B;
static constexpr uint8_t REG_PWM1_DUTY_H = 0x1C;
static constexpr uint8_t REG_PWM2_DUTY_L = 0x1D;
static constexpr uint8_t REG_PWM2_DUTY_H = 0x1E;
static constexpr uint8_t REG_PWM3_DUTY_L = 0x1F;
static constexpr uint8_t REG_PWM3_DUTY_H = 0x20;
static constexpr uint8_t REG_PWM4_DUTY_L = 0x21;
static constexpr uint8_t REG_PWM4_DUTY_H = 0x22;

// Configuration Registers
static constexpr uint8_t REG_I2C_CFG = 0x23;
static constexpr uint8_t REG_LED_CFG = 0x24;
static constexpr uint8_t REG_PWM_FREQ_L = 0x25;
static constexpr uint8_t REG_PWM_FREQ_H = 0x26;
static constexpr uint8_t REG_REF_VOLT_L = 0x27;
static constexpr uint8_t REG_REF_VOLT_H = 0x28;
static constexpr uint8_t REG_RESET = 0x29;

// Memory Registers
static constexpr uint8_t REG_LED_RAM_START = 0x30;
static constexpr uint8_t REG_RTC_RAM_START = 0x70;
static constexpr uint8_t REG_AW8737A_PLUSE = 0x90;

#define M5IOE1_ERR_FAILED(err) \
  if (!(err)) { \
    this->mark_failed(); \
    return; \
  }

void M5IOE1Component::setup() {
  ESP_LOGD(TAG, "Setting up M5IOE1...");
  // if reset is set, setup will take longer
  // there is internal logics to handle reset in M5IOE1
  if (this->reset_) {
    // software reset
    M5IOE1_ERR_FAILED(this->write_byte(REG_RESET, 0x3A));
    delay(1000);  // NOLINT: The device must finish resetting before setup continues.
  } else {
    delay(20);
  }

  // Read chip info
  uint8_t uid[2];
  uint8_t revision;
  M5IOE1_ERR_FAILED(this->read_bytes(REG_UID_L, uid, 2));
  M5IOE1_ERR_FAILED(this->read_byte(REG_VERSION, &revision));

  if (revision == 0x00 || revision == 0xFF) {
    ESP_LOGE(TAG, "Invalid version 0x%02X", revision);
    this->mark_failed();
    return;
  }

  this->device_uid_ = (uid[1] << 8) | uid[0];
  this->firm_ver_ = revision;

  // configure PWM frequency
  M5IOE1_ERR_FAILED(this->write_bytes(REG_PWM_FREQ_L, reinterpret_cast<uint8_t *>(&this->pwm_freq_), 2));

  // Attach GPIO Interrupt
  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    this->interrupt_pin_->attach_interrupt(&M5IOE1Component::gpio_intr, this, gpio::INTERRUPT_FALLING_EDGE);
    // Don't invalidate cache on read — only invalidate when interrupt fires
    this->set_invalidate_on_read_(false);
  }
  // Disable loop until an input pin is configured via pin_mode()
  // For interrupt-driven mode, loop is re-enabled by the ISR
  // For polling mode, loop is re-enabled when pin_mode() registers an input pin
  this->disable_loop();

  ESP_LOGD(TAG, "M5IOE1 Setup finished.");
}

void IRAM_ATTR M5IOE1Component::gpio_intr(M5IOE1Component *arg) { arg->enable_loop_soon_any_context(); }

// Clear the cache at the start of each loop.
void M5IOE1Component::loop() {
  // Invalidate the cache so the next digital_read() triggers a fresh I2C read
  this->reset_pin_cache_();
  // Only disable the loop once INT has actually gone HIGH. Input transitions that straddle the
  // I2C read leave INT asserted without re-firing a falling edge, which would strand us with
  // stale state forever; keep looping until the line is released so we self-heal.
  if (this->interrupt_pin_ != nullptr && this->interrupt_pin_->digital_read()) {
    this->disable_loop();
  }
}

void M5IOE1Component::dump_config() {
  ESP_LOGCONFIG(TAG,
                "M5IOE1 Config:\n"
                "  PWM Frequency: %d Hz\n"
                "  Device UID: %d\n"
                "  Firmware Version: %d",
                this->pwm_freq_, this->device_uid_, this->firm_ver_);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
}

void M5IOE1Component::pin_mode(uint8_t pin, gpio::Flags flags) {
  if (flags & gpio::FLAG_OUTPUT) {
    // Set mode mask bit
    this->mode_mask_ |= 1 << pin;
    if (flags & gpio::FLAG_OPEN_DRAIN) {
      this->drv_mode_mask_ |= 1 << pin;
    } else {
      this->drv_mode_mask_ &= ~(1 << pin);
    }
  } else if (flags & gpio::FLAG_INPUT) {
    // Clear mode mask bit
    this->mode_mask_ &= ~(1 << pin);
    if (flags & gpio::FLAG_PULLUP) {
      this->pull_up_mask_ |= 1 << pin;
    } else if (flags & gpio::FLAG_PULLDOWN) {
      this->pull_down_mask_ |= 1 << pin;
    }
  }

  // Write GPIO to enable input mode
  this->write_gpio_modes_();
}

bool M5IOE1Component::read_gpio_outputs_() {
  if (this->is_failed())
    return false;

  uint8_t data[2];

  if (!this->read_bytes(REG_GPIO_O_L, data, 2)) {
    this->status_set_warning(LOG_STR("Failed to read gpio output registers"));
    return false;
  }

  this->output_mask_ = data[0] | (data[1] << 8);
  this->status_clear_warning();
  return true;
}

bool M5IOE1Component::read_gpio_modes_() {
  if (this->is_failed())
    return false;

  uint8_t data[2];

  if (!this->read_bytes(REG_GPIO_M_L, data, 2)) {
    this->status_set_warning(LOG_STR("Failed to read GPIO modes"));
    return false;
  }

  uint16_t mode_mask = data[0] | (data[1] << 8);

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  ESP_LOGV(TAG, "Read GPIO modes : 0b" BYTE_TO_BINARY_PATTERN " " BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(data[1]),
           BYTE_TO_BINARY(data[0]));
#endif
  this->mode_mask_ = mode_mask;
  this->status_clear_warning();
  return true;
}

bool M5IOE1Component::write_gpio_modes_() {
  if (this->is_failed())
    return false;

  if (!this->write_bytes(REG_GPIO_M_L, reinterpret_cast<uint8_t *>(&this->mode_mask_), 2)) {
    this->status_set_warning(LOG_STR("Failed to write GPIO modes"));
    return false;
  }

  if (!this->write_bytes(REG_GPIO_PU_L, reinterpret_cast<uint8_t *>(&this->pull_up_mask_), 2)) {
    this->status_set_warning(LOG_STR("Failed to write GPIO pullup"));
    return false;
  }

  if (!this->write_bytes(REG_GPIO_PD_L, reinterpret_cast<uint8_t *>(&this->pull_down_mask_), 2)) {
    this->status_set_warning(LOG_STR("Failed to write GPIO pulldown"));
    return false;
  }

  if (!this->write_bytes(REG_GPIO_DRV_L, reinterpret_cast<uint8_t *>(&this->drv_mode_mask_), 2)) {
    this->status_set_warning(LOG_STR("Failed to write GPIO drive mode"));
    return false;
  }

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  ESP_LOGV(TAG, "Wrote GPIO modes: 0b" BYTE_TO_BINARY_PATTERN " " BYTE_TO_BINARY_PATTERN,
           BYTE_TO_BINARY(this->mode_mask_ >> 8), BYTE_TO_BINARY(this->mode_mask_ & 0xFF));
  ESP_LOGV(TAG, "Wrote GPIO pullup: 0b" BYTE_TO_BINARY_PATTERN " " BYTE_TO_BINARY_PATTERN,
           BYTE_TO_BINARY(this->pull_up_mask_ >> 8), BYTE_TO_BINARY(this->pull_up_mask_ & 0xFF));
  ESP_LOGV(TAG, "Wrote GPIO pulldown: 0b" BYTE_TO_BINARY_PATTERN " " BYTE_TO_BINARY_PATTERN,
           BYTE_TO_BINARY(this->pull_down_mask_ >> 8), BYTE_TO_BINARY(this->pull_down_mask_ & 0xFF));
  ESP_LOGV(TAG, "Wrote GPIO drive mode: 0b" BYTE_TO_BINARY_PATTERN " " BYTE_TO_BINARY_PATTERN,
           BYTE_TO_BINARY(this->drv_mode_mask_ >> 8), BYTE_TO_BINARY(this->drv_mode_mask_ & 0xFF));
#endif

  this->status_clear_warning();
  return true;
}

void M5IOE1Component::setup_gpio_interrupt(uint8_t pin, M5IOE1InterruptType type) {
  bool ret = true;
  ret &= this->enable_gpio_interrupt_(pin);
  ret &= this->config_interrupt_edge_(pin, type);
  if (!ret) {
    ESP_LOGW(TAG, "Failed to configure interrupt for pin %u", pin);
    return;
  }
}

bool M5IOE1Component::config_interrupt_edge_(uint8_t pin, M5IOE1InterruptType type) {
  // Step1: read current configuration
  uint8_t data[2];
  if (!this->read_bytes(REG_GPIO_IP_L, data, 2)) {
    this->status_set_warning(LOG_STR("Failed to read interrupt type register"));
    return false;
  }

  // Step2: set bit / clear bit
  uint16_t mask = (data[1] << 8) | data[0];

  if (type == RISING_EDGE) {
    mask |= (1 << pin);
  } else {
    mask &= ~(1 << pin);
  }

  // write back
  if (!this->write_bytes(REG_GPIO_IP_L, reinterpret_cast<uint8_t *>(&mask), 2)) {
    this->status_set_warning(LOG_STR("Failed to write interrupt type register"));
    return false;
  }

  this->status_clear_warning();
  return true;
}

bool M5IOE1Component::enable_gpio_interrupt_(uint8_t pin) {
  // Step 1: read current configuration
  uint8_t data[2];
  if (!this->read_bytes(REG_GPIO_IE_L, data, 2)) {
    this->status_set_warning(LOG_STR("Failed to read GPIO interrupt config register"));
    return false;
  }

  // Step 2: update cached value and set bit
  this->interrupt_config_mask_ = (data[1] << 8) | data[0];
  this->interrupt_config_mask_ |= (1 << pin);

  // Step3: write back
  if (!this->write_bytes(REG_GPIO_IE_L, reinterpret_cast<uint8_t *>(&this->interrupt_config_mask_), 2)) {
    this->status_set_warning(LOG_STR("Failed to write GPIO interrupt config register"));
    return false;
  }

  this->status_clear_warning();
  return true;
}

bool M5IOE1Component::disable_gpio_interrupt_(uint8_t pin) {
  // Step 1: read current configuration
  uint8_t data[2];
  if (!this->read_bytes(REG_GPIO_IE_L, data, 2)) {
    this->status_set_warning(LOG_STR("Failed to read GPIO interrupt config register"));
    return false;
  }

  // Step 2: update cached value and clear bit
  this->interrupt_config_mask_ = (data[1] << 8) | data[0];
  this->interrupt_config_mask_ &= ~(1 << pin);

  // Step3: write back
  if (!this->write_bytes(REG_GPIO_IE_L, reinterpret_cast<uint8_t *>(&this->interrupt_config_mask_), 2)) {
    this->status_set_warning(LOG_STR("Failed to write GPIO interrupt config register"));
    return false;
  }

  this->status_clear_warning();
  return true;
}

bool M5IOE1Component::read_gpio_interrupt_() {
  uint8_t data[2];
  if (!this->read_bytes(REG_GPIO_IS_L, data, 2)) {
    this->status_set_warning(LOG_STR("Failed to read interrupt register"));
    return false;
  }

  this->interrupt_result_mask_ = (data[1] << 8) | data[0];

  // Clear interrupt
  if (!this->clear_gpio_interrupt_()) {
    return false;
  }

  this->status_clear_warning();

  return true;
}

bool M5IOE1Component::clear_gpio_interrupt_() {
  uint8_t data[2] = {0x00, 0x00};
  if (!this->write_bytes(REG_GPIO_IS_L, data, 2)) {
    this->status_set_warning(LOG_STR("Failed to clear interrupt"));
    return false;
  }

  this->status_clear_warning();
  return true;
}

bool M5IOE1Component::digital_read_hw(uint8_t pin) {
  if (this->is_failed())
    return false;

  uint8_t data[2];

  if (!this->read_bytes(REG_GPIO_I_L, data, 2)) {
    this->status_set_warning(LOG_STR("Failed to read GPIO state"));
    return false;
  }

  this->input_mask_ = data[0] | (data[1] << 8);
  this->status_clear_warning();
  return true;
}

bool M5IOE1Component::digital_read_cache(uint8_t pin) { return (this->input_mask_ & (1 << pin)); }

void M5IOE1Component::digital_write_hw(uint8_t pin, bool value) {
  if (this->is_failed())
    return;

  // bit on operation
  if (value) {
    this->output_mask_ |= (1 << pin);
  } else {
    this->output_mask_ &= ~(1 << pin);
  }

  if (!this->write_bytes(REG_GPIO_O_L, reinterpret_cast<uint8_t *>(&this->output_mask_), 2)) {
    this->status_set_warning(LOG_STR("Failed to write output register"));
    return;
  }

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  ESP_LOGV(TAG, "Wrote GPIO output: 0b" BYTE_TO_BINARY_PATTERN " " BYTE_TO_BINARY_PATTERN,
           BYTE_TO_BINARY(this->output_mask_ >> 8), BYTE_TO_BINARY(this->output_mask_ & 0x00FF));
#endif
  this->status_clear_warning();
}

void M5IOE1Component::config_pwm_channel_(M5IOE1PWMChannel channel, M5IOE1PWMPolarity polar) {
  // PWM register
  uint8_t reg = REG_PWM1_DUTY_L + (static_cast<uint8_t>(channel) * 2);
  uint8_t data[2] = {0, 0};

  // set polarity
  if (polar == M5IOE1PWMPolarity::ACTIVE_LOW) {
    data[1] |= (1 << 6);
  } else {
    data[1] &= ~(1 << 6);
  }

  // set PWM enable
  data[1] |= (1 << 7);

  if (!this->write_bytes(reg, data, 2)) {
    this->status_set_warning(LOG_STR("Failed to configure PWM register."));
    return;
  }

  this->status_clear_warning();
}

void M5IOE1Component::write_pwm_duty_(M5IOE1PWMChannel channel, float duty) {
  uint8_t reg = REG_PWM1_DUTY_L + (static_cast<uint8_t>(channel) * 2);

  if (duty <= 0.0f)
    duty = 0.0f;
  if (duty >= 1.0f)
    duty = 1.0f;

  // calculate the uint16_t duty from given float range
  uint16_t value = static_cast<uint16_t>(duty * 4095.0f + 0.5f) & 0x0FFF;

  if (!this->write_bytes(reg, reinterpret_cast<uint8_t *>(&value), 2)) {
    this->status_set_warning(LOG_STR("Failed to set PWM duty register."));
    return;
  }

  delay(5);

  this->status_clear_warning();
}

float M5IOE1Component::read_adc_(M5IOE1ADCChannel channel) {
  if (channel == REF_VOLT) {
    return this->get_adc_ref_voltage();
  }

  // Select the channel and start a 12-bit conversion in the same write.
  uint8_t data = static_cast<uint8_t>(channel) | (1 << 6);

  // Configure the register to start ADC sampling
  if (!this->write_byte(REG_ADC_CTRL, data)) {
    this->status_set_warning(LOG_STR("Failed to configure ADC register for sampling."));
    return NAN;
  }

  // wait for ADC sampling
  delay(5);

  // read sample result
  uint8_t result[2];
  if (!this->read_bytes(REG_ADC_D_L, result, 2)) {
    this->status_set_warning(LOG_STR("Failed to read ADC value"));
    return NAN;
  } else {
    // H [11:8], L[7:0]
    uint16_t val = ((result[1] & 0x0F) << 8) | result[0];
    return static_cast<float>(val);
  }

  return NAN;
}

float M5IOE1Component::read_adc_ref_() {
  uint8_t data[2];

  if (!this->read_bytes(REG_REF_VOLT_L, data, 2)) {
    this->status_set_warning(LOG_STR("Failed to read ADC reference register"));
    return NAN;
  }

  this->status_clear_warning();
  return static_cast<float>(data[1] << 8 | data[0]);
}

float M5IOE1Component::read_temperature_() {
  uint8_t data = 0;
  // enable TSTART, this will start sampling
  data |= (1 << 6);

  // write back control value
  if (!this->write_byte(REG_TEMP_CTRL, data)) {
    this->status_set_warning(LOG_STR("Failed to write temperature config register."));
    return NAN;
  }

  // wait for sampling
  delay(5);

  // Read TBUSY signal
  if (!this->read_byte(REG_TEMP_CTRL, &data)) {
    this->status_set_warning(LOG_STR("Failed to read temperature config register."));
    return NAN;
  }

  bool busy = static_cast<bool>(data >> 7);

  // TBUSY = 1 - sampling
  // TBUSY = 0 - finished
  if (busy) {
    ESP_LOGD(TAG, "Temperature is still sampling, skip the reading");
    return NAN;
  } else {
    // Read temperature registers
    uint8_t data[2];
    if (!this->read_bytes(REG_TEMP_D_L, data, 2)) {
      this->status_set_warning(LOG_STR("Failed to read temperature value"));
      return NAN;
    }

    // Concat the value, REG_TEMP_D_H [3:0] + REG_TEMP_D_L [7:0]
    uint16_t raw = ((data[1] & 0x0F) << 8) | data[0];

    return static_cast<float>(raw);
  }

  return NAN;
}

void M5IOE1Component::config_neopixel_(uint8_t num) {
  uint8_t data;

  // read REG_LED_CFG
  if (!this->read_byte(REG_LED_CFG, &data)) {
    this->status_set_warning(LOG_STR("Failed to read LED configuration register"));
    return;
  }

  // clear bits on [5:0]
  data &= ~0x3F;

  // set number of LEDs
  data |= num;

  // bit on refresh, refresh on setup
  data |= (1 << 6);

  // write LED configuration
  if (!this->write_byte(REG_LED_CFG, data)) {
    this->status_set_warning(LOG_STR("Failed to write LED configuration register"));
    return;
  }

  this->status_clear_warning();
}

void M5IOE1Component::write_led_ram_(const uint8_t *buf, size_t len) {
  if (!this->write_bytes(REG_LED_RAM_START, buf, len)) {
    this->status_set_warning(LOG_STR("Failed to write data to LED RAM"));
    return;
  }

  // Enable refresh
  uint8_t data;
  if (!this->read_byte(REG_LED_CFG, &data)) {
    this->status_set_warning(LOG_STR("Failed to read LED configuration register."));
    return;
  }

  data |= (1 << 6);

  if (!this->write_byte(REG_LED_CFG, data)) {
    this->status_set_warning(LOG_STR("Failed to write LED configuration register."));
    return;
  }

  delay(30);

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  char logbuf[256];
  size_t pos = 0;

  for (size_t i = 0; i < len && pos < sizeof(logbuf) - 4; i++) {
    pos += snprintf(logbuf + pos, sizeof(logbuf) - pos, "%02X ", buf[i]);
  }

  ESP_LOGV(TAG, "Write LED Buffer[%u]: %s", len, logbuf);

#endif
  this->status_clear_warning();
}

void M5IOE1Component::config_aw8737a_(uint8_t pin, uint8_t pulse_num) {
  uint8_t data = 0;

  data |= pin;  // which pin to use (0 - 13, corresponding IO1 - IO14)

  data |= (pulse_num << 5);  // pulse count, NUM[1:0] at bits 6:5

  data &= ~(1 << 7);  // wait for the next trigger

  if (!this->write_byte(REG_AW8737A_PLUSE, data)) {
    this->status_set_warning(LOG_STR("Failed to write pulse register"));
    return;
  }

  this->status_clear_warning();
}

void M5IOE1Component::enable_aw8737a_pluse_(bool enabled) {
  uint8_t data;
  // read pulse control register
  if (!this->read_byte(REG_AW8737A_PLUSE, &data)) {
    this->status_set_warning(LOG_STR("Failed to read pulse control register"));
    return;
  }

  if (enabled) {
    data |= (1 << 7);  // commence pulse generation immediately
  } else {
    data &= ~(1 << 7);  // do not trigger refresh, wait for the next trigger
  }

  // write back

  if (!this->write_byte(REG_AW8737A_PLUSE, data)) {
    this->status_set_warning(LOG_STR("Failed to write pulse control register"));
    return;
  }

  this->status_clear_warning();
}

void M5IOE1GPIOPin::setup() {
  this->pin_mode(this->flags_);
  // Enable GPIO interrupt and configure interrupt edge
  if (this->use_interrupt_) {
    this->parent_->setup_gpio_interrupt(this->pin_, this->interrupt_type_);
  }
}

void M5IOE1GPIOPin::pin_mode(gpio::Flags flags) { this->parent_->pin_mode(this->pin_, flags); }

bool M5IOE1GPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }

void M5IOE1GPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }

size_t M5IOE1GPIOPin::dump_summary(char *buffer, size_t len) const {
  return snprintf(buffer, len,
                  "%u via M5IOE1\n"
                  "  use interrupt: %s\n"
                  "  interrupt type: %s",
                  this->pin_, TRUEFALSE(this->use_interrupt_),
                  this->interrupt_type_ == 0 ? "FALLING EDGE" : "RISING EDGE");
}

}  // namespace esphome::m5ioe1
