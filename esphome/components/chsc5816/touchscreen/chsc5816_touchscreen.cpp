#include "chsc5816_touchscreen.h"
#include "CHSC5816Constants.h"

namespace esphome {
namespace chsc5816 {

int CHSC5816Touchscreen::writeRegister(uint32_t reg, uint8_t *buf, uint8_t length, bool stop = true) {
  i2c::WriteBuffer buffers[2];
  buffers[0].data = reinterpret_cast<const uint8_t *>(reg);
  buffers[0].len = __reg_addr_len;
  buffers[1].data = buf;
  buffers[1].len = length;
  return this->bus_->writev(this->address_, buffers, 2, stop);
}

int CHSC5816Touchscreen::readRegister(uint32_t reg, uint8_t *buf, uint8_t length, bool stop = true) {
  this->write(reinterpret_cast<const uint8_t *>(reg), __reg_addr_len, stop);
  return bus_->read(address_, buf, length);
}

void CHSC5816Touchscreen::reset() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(5);
    this->reset_pin_->digital_write(true);
    delay(5);
  }
}

bool CHSC5816Touchscreen::checkOnline() {
  CHSC5816_Header_t tmp;
  memset(&tmp, 0, sizeof(CHSC5816_Header_t));
  memset(&__header, 0, sizeof(CHSC5816_Header_t));

  uint32_t bootClean = 0x00;
  writeRegister(CHSC5816_REG_BOOT_STATE, (uint8_t *) &bootClean, 4);

  this->reset();

  for (int i = 0; i < 10; ++i) {
    esph_log_config(TAG, "ATTEMPT %d", i);
    delay(10);
    readRegister(CHSC5816_REG_IMG_HEAD, (uint8_t *) &__header, sizeof(CHSC5816_Header_t));
    readRegister(CHSC5816_REG_IMG_HEAD, (uint8_t *) &tmp, sizeof(CHSC5816_Header_t));

    esph_log_config(TAG, "H1 %s",
                    format_hex_pretty(reinterpret_cast<const uint8_t *>(&__header), sizeof(CHSC5816_Header_t)).c_str());
    esph_log_config(TAG, "H2 %s",
                    format_hex_pretty(reinterpret_cast<const uint8_t *>(&tmp), sizeof(CHSC5816_Header_t)).c_str());
    if (memcmp(&tmp, &__header, sizeof(CHSC5816_Header_t)) != 0) {
      continue;
    }
    if (__header.sig == CHSC5816_SIG_VALUE) {
      return true;
    }
  }
  return false;
}

void CHSC5816Touchscreen::continue_setup_() {
  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);
  }
  if (!this->checkOnline()) {
    esph_log_e(TAG, "Failed to setup");
  } else {
    esph_log_config(TAG, "CHSC5816 Touchscreen setup complete");
  }

  return;  // Remove this after we're successfully online

  this->write_byte(REG_IRQ_CTL, IRQ_EN_MOTION);
  if (this->x_raw_max_ == this->x_raw_min_) {
    this->x_raw_max_ = this->display_->get_native_width();
  }
  if (this->y_raw_max_ == this->y_raw_min_) {
    this->y_raw_max_ = this->display_->get_native_height();
  }
  esph_log_config(TAG, "CHSC5816 Touchscreen setup complete");
}

void CHSC5816Touchscreen::update_button_state_(bool state) {
  if (this->button_touched_ == state)
    return;
  this->button_touched_ = state;
  for (auto *listener : this->button_listeners_)
    listener->update_button(state);
}

void CHSC5816Touchscreen::setup() {
  esph_log_config(TAG, "Setting up CHSC5816 Touchscreen...");
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
  }

  this->reset();

  if (this->reset_pin_ != nullptr) {
    // this->set_timeout(30, [this] { this->continue_setup_(); });
    this->set_timeout(10, [this] { this->continue_setup_(); });
  } else {
    this->continue_setup_();
  }
}

void CHSC5816Touchscreen::update_touches() {
  uint8_t data[13];
  if (!this->read_bytes(REG_STATUS, data, sizeof data)) {
    this->status_set_warning();
    return;
  }
  uint8_t num_of_touches = data[REG_TOUCH_NUM] & 3;
  if (num_of_touches == 0) {
    this->update_button_state_(false);
    return;
  }

  uint16_t x = encode_uint16(data[REG_XPOS_HIGH] & 0xF, data[REG_XPOS_LOW]);
  uint16_t y = encode_uint16(data[REG_YPOS_HIGH] & 0xF, data[REG_YPOS_LOW]);
  esph_log_v(TAG, "Read touch %d/%d", x, y);
  if (x >= this->x_raw_max_) {
    this->update_button_state_(true);
  } else {
    this->add_raw_touch_position_(0, x, y);
  }
}

void CHSC5816Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "CHSC5816 Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
}

}  // namespace chsc5816
}  // namespace esphome
