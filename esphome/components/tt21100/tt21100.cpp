#include "tt21100.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tt21100 {

static const char *const TAG = "tt21100";

void TT21100TouchscreenStore::gpio_intr(TT21100TouchscreenStore *store) { store->touch = true; }

float TT21100Touchscreen::get_setup_priority() const { return setup_priority::HARDWARE - 1.0f; }

void TT21100Touchscreen::setup() {
  ESP_LOGCONFIG(TAG, "Setting up TT21100 Touchscreen...");

  // Register interrupt pin
  this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  this->interrupt_pin_->setup();
  this->store_.pin = this->interrupt_pin_->to_isr();
  this->interrupt_pin_->attach_interrupt(TT21100TouchscreenStore::gpio_intr, &this->store_,
                                         gpio::INTERRUPT_FALLING_EDGE);

  // Perform reset if necessary
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_();
  }

  // Update display dimensions if they were updated during display setup
  this->display_width_ = this->display_->get_width_internal();
  this->display_height_ = this->display_->get_height_internal();
  this->rotation_ = static_cast<TouchRotation>(this->display_->get_rotation());

  // Trigger initial read to activate the interrupt
  this->store_.touch = true;
}

void TT21100Touchscreen::loop() {
  if (!this->store_.touch)
    return;
  this->store_.touch = false;

  // Read report length
  uint16_t data_len;
  this->read((uint8_t *) &data_len, sizeof(data_len));

  // Read report data
  uint8_t data[MAX_DATA_LEN];
  if (data_len > 0 && data_len < sizeof(data)) {
    this->read(data, data_len);

    if (data_len == 14) {
      // Button event
      auto *report = (TT21100ButtonReport *) data;

      ESP_LOGV(TAG, "Button report: Len=%d, ID=%d, Time=%5u, Val=[%u] - [%04X][%04X][%04X][%04X]", report->length,
               report->report_id, report->timestamp, report->btn_val, report->btn_signal[0], report->btn_signal[1],
               report->btn_signal[2], report->btn_signal[3]);

      for (int i = 0; i < 4; i++) {
        if (this->buttons_[i] != nullptr) {
          this->buttons_[i]->publish_state(report->btn_signal[i] > 0);
        }
      }

    } else if (data_len >= 7) {
      // Touch point event
      auto *report = (TT21100TouchReport *) data;

      ESP_LOGV(TAG,
               "Touch report: Len=%d, ID=%d, Time=%5u, LargeObject=%u, RecordNum=%u, RecordCounter=%u, NoiseEfect=%u",
               report->length, report->report_id, report->timestamp, report->large_object, report->record_num,
               report->report_counter, report->noise_efect);

      uint8_t touch_count = (data_len - sizeof(TT21100TouchReport)) / sizeof(TT21100TouchRecord);

      if (touch_count == 0) {
        for (auto *listener : this->touch_listeners_)
          listener->release();
        return;
      }

      for (int i = 0; i < touch_count; i++) {
        auto *touch = &report->touch_record[i];

        ESP_LOGV(TAG,
                 "Touch %d: Type=%u, Tip=%u, EventId=%u, TouchId=%u, X=%u, Y=%u, Pressure=%u, MajorAxisLen=%u, "
                 "Orientation=%u",
                 i, touch->touch_type, touch->tip, touch->event_id, touch->touch_id, touch->x, touch->y,
                 touch->pressure, touch->major_axis_length, touch->orientation);

        TouchPoint tp;
        switch (this->rotation_) {
          case ROTATE_0_DEGREES:
            // Origin is top right, so mirror X by default
            tp.x = this->display_width_ - touch->x;
            tp.y = touch->y;
            break;
          case ROTATE_90_DEGREES:
            tp.x = touch->y;
            tp.y = touch->x;
            break;
          case ROTATE_180_DEGREES:
            tp.x = touch->x;
            tp.y = this->display_height_ - touch->y;
            break;
          case ROTATE_270_DEGREES:
            tp.x = this->display_height_ - touch->y;
            tp.y = this->display_width_ - touch->x;
            break;
        }
        tp.id = touch->tip;
        tp.state = touch->pressure;

        this->defer([this, tp]() { this->send_touch_(tp); });
      }
    }
  }
}

void TT21100Touchscreen::reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(10);
    this->reset_pin_->digital_write(true);
    delay(10);
  }
}

void TT21100Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "TT21100 Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_BINARY_SENSOR("  ", "Button 1", this->buttons_[0]);
  LOG_BINARY_SENSOR("  ", "Button 2", this->buttons_[1]);
  LOG_BINARY_SENSOR("  ", "Button 3", this->buttons_[2]);
  LOG_BINARY_SENSOR("  ", "Button 4", this->buttons_[3]);
}

}  // namespace tt21100
}  // namespace esphome
