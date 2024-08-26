#include "ch422g.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ch422g {

const uint8_t CH422G_REG_IN = 0x26;
const uint8_t CH422G_REG_OUT = 0x38;
const uint8_t OUT_REG_DEFAULT_VAL = 0xdf;

static const char *const TAG = "ch422g";

void CH422GComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up CH422G...");
  // Test to see if device exists
  if (!this->read_inputs_()) {
    ESP_LOGE(TAG, "CH422G not detected at 0x%02X", this->address_);
    this->mark_failed();
    return;
  }

  // restore defaults over whatever got saved on last boot
  // this->write_output_(OUT_REG_DEFAULT_VAL);

  ESP_LOGD(TAG, "Initialization complete. Warning: %d, Error: %d", this->status_has_warning(),
           this->status_has_error());
}

void CH422GComponent::loop() {
  // The read_inputs_() method will cache the input values from the chip.
  this->read_inputs_();
  // Clear all the previously read flags.
  this->was_previously_read_ = 0x00;
}

void CH422GComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "CH422G:");
  LOG_I2C_DEVICE(this)
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with CH422G failed!");
  }
}

// ch422g doesn't have any flag support
void CH422GComponent::pin_mode(uint8_t pin, gpio::Flags flags) {}

bool CH422GComponent::digital_read(uint8_t pin) {
  // Note: We want to try and avoid doing any I2C bus read transactions here
  // to conserve I2C bus bandwidth. So what we do is check to see if we
  // have seen a read during the time esphome is running this loop. If we have,
  // we do an I2C bus transaction to get the latest value. If we haven't
  // we return a cached value which was read at the time loop() was called.
  if (this->was_previously_read_ & (1 << pin))
    this->read_inputs_();  // Force a read of a new value
  // Indicate we saw a read request for this pin in case a
  // read happens later in the same loop.
  this->was_previously_read_ |= (1 << pin);

  ESP_LOGV(TAG, "digital_read(%d). Mask: %d, Contains: %d", (int) pin, (int) this->state_mask_,
           (int) (this->state_mask_ & (1 << pin)));
  return this->state_mask_ & (1 << pin);
}

bool CH422GComponent::read_inputs_() {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Device marked failed");
    return false;
  }

  uint8_t temp = 0;
  if ((this->last_error_ = this->read(&temp, 1)) != esphome::i2c::ERROR_OK) {
    this->status_set_warning();
    ESP_LOGE(TAG, "read_register_(): I2C I/O error: %d", (int) this->last_error_);
    return false;
  }

  uint8_t output = 0;
  if ((this->last_error_ = this->bus_->read(CH422G_REG_IN, &output, 1)) != esphome::i2c::ERROR_OK) {
    this->status_set_warning();
    ESP_LOGE(TAG, "read_register_(): I2C I/O error: %d", (int) this->last_error_);
    return false;
  }

  ESP_LOGV(TAG, "read_inputs_() output: %d", (int) output);
  this->state_mask_ = output;
  this->status_clear_warning();

  return true;
}

void CH422GComponent::digital_write(uint8_t pin, bool value) {
  if (value) {
    this->state_mask_ |= (1 << pin);
  } else {
    this->state_mask_ &= ~(1 << pin);
  }
  ESP_LOGV(TAG, "digital_write(%d, %d) mask: %d", (int) pin, (int) value, (int) this->state_mask_);
  this->write_output_(this->state_mask_);
}

bool CH422GComponent::write_output_(uint8_t value) {
  const uint8_t temp = 1;
  if ((this->last_error_ = this->write(&temp, 1, false)) != esphome::i2c::ERROR_OK) {
    this->status_set_warning();
    ESP_LOGE(TAG, "write_output_(): I2C I/O error: %d", (int) this->last_error_);
    return false;
  }

  uint8_t writeValue = value;
  if ((this->last_error_ = this->bus_->write(CH422G_REG_OUT, &writeValue, 1)) != esphome::i2c::ERROR_OK) {
    this->status_set_warning();
    ESP_LOGE(TAG, "write_output_(): I2C I/O error: %d writeValue: %d", (int) this->last_error_, (int) writeValue);
    return false;
  }

  this->state_mask_ = value;
  this->status_clear_warning();
  return true;
}

float CH422GComponent::get_setup_priority() const { return setup_priority::IO; }

// Run our loop() method very early in the loop, so that we cache read values
// before other components call our digital_read() method.
float CH422GComponent::get_loop_priority() const { return 9.0f; }  // Just after WIFI

void CH422GGPIOPin::setup() { pin_mode(flags_); }
void CH422GGPIOPin::pin_mode(gpio::Flags flags) { this->parent_->pin_mode(this->pin_, flags); }
bool CH422GGPIOPin::digital_read() {
  bool ret = this->parent_->digital_read(this->pin_);
  ESP_LOGV(TAG, "Reading state of pin %d: %d", (int) this->pin_, (int) ret);
  return ret != this->inverted_;
}

void CH422GGPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }
std::string CH422GGPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%u via CH422G", pin_);
  return buffer;
}

}  // namespace ch422g
}  // namespace esphome
