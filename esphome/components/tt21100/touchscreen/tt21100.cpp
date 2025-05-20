#include "tt21100.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tt21100 {

static const char *const TAG = "tt21100";

static const uint8_t MAX_BUTTONS = 4;
static const uint8_t MAX_TOUCH_POINTS = 5;
static const uint8_t MAX_DATA_LEN = (7 + MAX_TOUCH_POINTS * 10);  // 7 Header + (Points * 10 data bytes)

struct TT21100ButtonReport {
  uint16_t length;     // Always 14 (0x000E)
  uint8_t report_id;   // Always 0x03
  uint16_t timestamp;  // Number in units of 100 us
  uint8_t btn_value;   // Only use bit 0..3
  uint16_t btn_signal[MAX_BUTTONS];
} __attribute__((packed));

struct TT21100TouchRecord {
  uint8_t : 5;
  uint8_t touch_type : 3;
  uint8_t tip : 1;
  uint8_t event_id : 2;
  uint8_t touch_id : 5;
  uint16_t x;
  uint16_t y;
  uint8_t pressure;
  uint16_t major_axis_length;
  uint8_t orientation;
} __attribute__((packed));

struct TT21100TouchReport {
  uint16_t length;
  uint8_t report_id;
  uint16_t timestamp;
  uint8_t : 2;
  uint8_t large_object : 1;
  uint8_t record_num : 5;
  uint8_t report_counter : 2;
  uint8_t : 3;
  uint8_t noise_effect : 3;
  TT21100TouchRecord touch_record[MAX_TOUCH_POINTS];
} __attribute__((packed));

float TT21100Touchscreen::get_setup_priority() const { return setup_priority::HARDWARE - 1.0f; }

void TT21100Touchscreen::setup() {
  ESP_LOGCONFIG(TAG, "Setting up TT21100 Touchscreen...");

  // Register interrupt pin
  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
    this->interrupt_pin_->setup();
    this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);
  }

  // Perform reset if necessary
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_();
  }

  // Update display dimensions if they were updated during display setup
  if (this->display_ != nullptr) {
    if (this->x_raw_max_ == this->x_raw_min_) {
      this->x_raw_max_ = this->display_->get_native_width();
    }
    if (this->y_raw_max_ == this->y_raw_min_) {
      this->y_raw_max_ = this->display_->get_native_height();
    }
  }

  // Trigger initial read to activate the interrupt
  this->store_.touched = true;
}

void TT21100Touchscreen::update_touches() {
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

      ESP_LOGV(TAG, "Button report: Len=%d, ID=%d, Time=%5u, Value=[%u], Signal=[%04X][%04X][%04X][%04X]",
               report->length, report->report_id, report->timestamp, report->btn_value, report->btn_signal[0],
               report->btn_signal[1], report->btn_signal[2], report->btn_signal[3]);

      for (uint8_t i = 0; i < 4; i++) {
        for (auto *listener : this->button_listeners_)
          listener->update_button(i, report->btn_signal[i]);
      }

    } else if (data_len >= 7) {
      // Touch point event
      auto *report = (TT21100TouchReport *) data;

      ESP_LOGV(TAG,
               "Touch report: Len=%d, ID=%d, Time=%5u, LargeObject=%u, RecordNum=%u, RecordCounter=%u, NoiseEffect=%u",
               report->length, report->report_id, report->timestamp, report->large_object, report->record_num,
               report->report_counter, report->noise_effect);

      uint8_t touch_count = (data_len - (sizeof(*report) - sizeof(report->touch_record))) / sizeof(TT21100TouchRecord);

      for (int i = 0; i < touch_count; i++) {
        auto *touch = &report->touch_record[i];

        ESP_LOGV(TAG,
                 "Touch %d: Type=%u, Tip=%u, EventId=%u, TouchId=%u, X=%u, Y=%u, Pressure=%u, MajorAxisLen=%u, "
                 "Orientation=%u",
                 i, touch->touch_type, touch->tip, touch->event_id, touch->touch_id, touch->x, touch->y,
                 touch->pressure, touch->major_axis_length, touch->orientation);

        this->add_raw_touch_position_(touch->tip, touch->x, touch->y, touch->pressure);
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
}

}  // namespace tt21100
}  // namespace esphome
