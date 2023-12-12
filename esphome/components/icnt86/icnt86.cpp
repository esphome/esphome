#include "icnt86.h"
#include "esphome/core/log.h"

namespace esphome {
namespace icnt86 {

static const char *const TAG = "icnt86";

uint8_t old_X = 0;
uint8_t old_Y = 0;

#define UBYTE uint8_t
#define UWORD uint16_t
#define UDOUBLE uint32_t

void ICNT86TouchscreenStore::gpio_intr(ICNT86TouchscreenStore *store) { store->touch = true; }

float ICNT86Touchscreen::get_setup_priority() const { return setup_priority::HARDWARE - 1.0f; }

void ICNT86Touchscreen::setup() {
  ESP_LOGCONFIG(TAG, "Setting up icnt86 Touchscreen...");

  // Register interrupt pin
  this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  this->interrupt_pin_->setup();
  this->store_.pin = this->interrupt_pin_->to_isr();
  this->interrupt_pin_->attach_interrupt(ICNT86TouchscreenStore::gpio_intr, &this->store_,
                                         gpio::INTERRUPT_FALLING_EDGE);

  // Perform reset if necessary
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_();
  }

  // Update display dimensions if they were updated during display setup
  this->display_width_ = this->display_->get_width();
  this->display_height_ = this->display_->get_height();
  this->rotation_ = static_cast<TouchRotation>(this->display_->get_rotation());

  // Trigger initial read to activate the interrupt
  this->store_.touch = true;
}

void ICNT86Touchscreen::loop() {
  if (!this->store_.touch) {
          for (auto *listener : this->touch_listeners_)
        listener->release();
    return;
  }
  ESP_LOGD(TAG, "touch");

  this->store_.touch = false;

  char buf[100];
  char mask[1] = {0x00};
  // Read report length
  uint16_t data_len;

  this->ICNT_Read_(0x1001, buf, 1);
  uint8_t touch_count = buf[0];
  ESP_LOGD(TAG, "Touch count: %d", touch_count);

  if (buf[0] == 0x00) {  // No new touch
    this->ICNT_Write_(0x1001, mask, 1);
    delay(1);
    for (auto *listener : this->touch_listeners_)
      listener->release();
    return;
  } else {
    if (touch_count > 5 || touch_count < 1) {
      this->ICNT_Write_(0x1001, mask, 1);
      touch_count = 0;
      for (auto *listener : this->touch_listeners_)
        listener->release();
      return;
    }
    this->ICNT_Read_(0x1002, buf, touch_count * 7);
    this->ICNT_Write_(0x1001, mask, 1);

    for (UBYTE i = 0; i < touch_count; i++) {
      UWORD X = ((UWORD) buf[2 + 7 * i] << 8) + buf[1 + 7 * i];
      UWORD Y = ((UWORD) buf[4 + 7 * i] << 8) + buf[3 + 7 * i];
      UWORD P = buf[5 + 7 * i];
      UWORD TouchEvenid = buf[6 + 7 * i];

      if (old_X == X || old_Y == Y) {
        continue;
      }

      TouchPoint tp;
      switch (this->rotation_) {
        case ROTATE_0_DEGREES:
          // Origin is top right, so mirror X by default
          tp.x = this->display_width_ - X;
          tp.y = Y;
          break;
        case ROTATE_90_DEGREES:
          tp.x = X;
          tp.y = Y;
          break;
        case ROTATE_180_DEGREES:
          tp.x = Y;
          tp.y = this->display_height_ - Y;
          break;
        case ROTATE_270_DEGREES:
          tp.x = this->display_height_ - Y;
          tp.y = this->display_width_ - X;
          break;
      }
      tp.id = TouchEvenid;
      tp.state = P;

      ESP_LOGD(TAG, "Touch x: %d, y: %d, p: %d", X, Y, P);

      this->defer([this, tp]() { this->send_touch_(tp); });
    }
  }
}

void ICNT86Touchscreen::reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(10);
    this->reset_pin_->digital_write(true);
    delay(10);
  }
}

void ICNT86Touchscreen::I2C_Write_Byte_(UWORD Reg, char *Data, UBYTE len) {
  char wbuf[50] = {(Reg >> 8) & 0xff, Reg & 0xff};
  for (UBYTE i = 0; i < len; i++) {
    wbuf[i + 2] = Data[i];
  }
  this->write((const uint8_t *) wbuf, len + 2);
}

void ICNT86Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "icnt86 Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
}

void ICNT86Touchscreen::ICNT_Read_(UWORD Reg, char *Data, UBYTE len) { this->I2C_Read_Byte_(Reg, Data, len); }

void ICNT86Touchscreen::ICNT_Write_(UWORD Reg, char *Data, UBYTE len) { this->I2C_Write_Byte_(Reg, Data, len); }
void ICNT86Touchscreen::I2C_Read_Byte_(UWORD Reg, char *Data, UBYTE len) {
  char *rbuf = Data;
  this->I2C_Write_Byte_(Reg, 0, 0);
  this->read((uint8_t *) rbuf, len);
}

}  // namespace icnt86
}  // namespace esphome
