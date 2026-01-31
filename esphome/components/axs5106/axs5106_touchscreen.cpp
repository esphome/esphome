#include "axs5106_touchscreen.h"

namespace esphome {
namespace axs5106 {

static const char *const TAG = "axs5106.touchscreen";

const uint8_t TOUCH_AXS5106_TOUCH_POINTS_REG = 0x01;
const uint8_t TOUCH_AXS5106_ID_REG = 0x08;

const uint8_t AXS5106_ID[] = {0x51, 0x06, 0x01};

enum GestureId {
  GESTURE_NONE = 0x00,              // No gesture
  GESTURE_SWIPE_UP = 0x01,          // Swipe up
  GESTURE_SWIPE_DOWN = 0x02,        // Swipe down
  GESTURE_SWIPE_LEFT = 0x03,        // Swipe left
  GESTURE_SWIPE_RIGHT = 0x04,       // Swipe right
  GESTURE_SINGLE_TAP = 0x05,        // Single tap
  GESTURE_DOUBLE_TAP = 0x0B,        // Double tap
  GESTURE_LONG_PRESS = 0x0C,        // Long press
  GESTURE_SINGLE_TAP_WAKE = 0x10,   // Single tap to wake (requires LPWG mode enabled)
  GESTURE_ZOOM_OUT = 0x20,          // Zoom out
  GESTURE_ZOOM_IN = 0x21,           // Zoom in
  GESTURE_KEY_DOWN = 0x30,          // Key down
  GESTURE_KEY_UP = 0x31,            // Key up
  GESTURE_PALM_TOUCH_START = 0x80,  // Large-area touch start
  GESTURE_PALM_TOUCH_END = 0x90     // Large-area touch end
};

enum EventId {
  EVENT_DOWN = (0b00),     // 00 in bits 7:6 → Down event (start of touch)
  EVENT_UP = (0b01),       // 01 in bits 7:6 → Up event (end of touch)
  EVENT_CONTACT = (0b10),  // 10 in bits 7:6 → Contact event (during touch)
  EVENT_RESERVED = (0b11)  // 11 in bits 7:6 → Reserved
};

union GesturePacket {
  uint8_t raw[14];  // raw byte access

  struct {
    uint8_t gesture_id;    // Data0
    uint8_t touch_number;  // Data1

    // ---- Point 0 ----
    struct {
      union {
        uint8_t x_high_raw;  // Data2
        struct {
          uint8_t x_high : 4;    // bits [3:0] = X[11:8]
          uint8_t reserved : 2;  // bits [5:4]
          uint8_t event : 2;     // bits [7:6]
        };
      };
      uint8_t x_low;  // Data3: X[7:0]

      union {
        uint8_t y_high_raw;  // Data4
        struct {
          uint8_t y_high : 4;  // bits [3:0] = Y[11:8]
          uint8_t id : 4;      // bits [7:4]
        };
      };
      uint8_t y_low;  // Data5: Y[7:0]

      uint8_t weight;  // Data6
      uint8_t area;    // Data7
    } point0;

    // ---- Point 1 ----
    struct {
      union {
        uint8_t x_high_raw;  // Data8
        struct {
          uint8_t x_high : 4;    // bits [3:0] = X[11:8]
          uint8_t reserved : 2;  // bits [5:4]
          uint8_t event : 2;     // bits [7:6]
        };
      };
      uint8_t x_low;  // Data9

      union {
        uint8_t y_high_raw;  // Data10
        struct {
          uint8_t y_high : 4;  // bits [3:0] = Y[11:8]
          uint8_t id : 4;      // bits [7:4]
        };
      };
      uint8_t y_low;  // Data11

      uint8_t weight;  // Data12
      uint8_t area;    // Data13
    } point1;

  } fields;
};

void AXS5106Touchscreen::setup() {
  this->reset_pin_->setup();
  this->reset_pin_->digital_write(false);

  this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  this->interrupt_pin_->setup();
  this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);

  if (this->x_raw_max_ == this->x_raw_min_) {
    this->x_raw_max_ = this->display_->get_native_width();
  }
  if (this->y_raw_max_ == this->y_raw_min_) {
    this->y_raw_max_ = this->display_->get_native_height();
  }

  // Take device out of reset off the main loop
  this->set_timeout(10, [this] { this->reset_pin_->digital_write(true); });
  // Then try reading the ID
  this->set_timeout(30, [this] { this->continue_setup_(); });
}

/* Confirm the ID reg is what we expect, before marking
 * this ready to go.
 */
void AXS5106Touchscreen::continue_setup_() {
  uint8_t res[3] = {0};
  // Query ID register
  i2c::ErrorCode err = this->write(&TOUCH_AXS5106_ID_REG, 1);
  if (err != i2c::ERROR_OK) {
    this->mark_failed();
    ESP_LOGE(TAG, "Read chip ID failed");
    return;
  }
  delayMicroseconds(45);
  this->read_bytes_raw(res, sizeof res);  // ID is three bytes

  for (unsigned int i = 0; i < sizeof res; i++) {
    ESP_LOGCONFIG(TAG, "  ID[%u]=%02x", i, res[i]);
  }

  if (0 != memcmp(res, AXS5106_ID, sizeof AXS5106_ID)) {
    ESP_LOGE(TAG, "Chip ID mismatch, not continuing");
    this->mark_failed();
  } else {
    this->setup_complete_ = true;
  }
}

void AXS5106Touchscreen::update_touches() {
  GesturePacket gp{{0}};

  /* This bit is a little stupid.  You can't use `read_register` here
   * because the micro needs a little rest before actually having
   * the data ready.
   *
   * The datasheet for the CST5106L says to wait 45us and then you'll
   * still get a NACK
   */
  i2c::ErrorCode err = this->write(&TOUCH_AXS5106_TOUCH_POINTS_REG, 1);
  if (err != i2c::ERROR_OK) {
    this->status_set_warning(ESP_LOG_MSG_COMM_FAIL);
    this->skip_update_ = true;
    ESP_LOGE(TAG, "Read failed");
    return;
  }
  delayMicroseconds(45);
  this->read_bytes_raw(gp.raw, sizeof gp.raw);

  this->status_clear_warning();

  for (int i = 0; i < 14; i++) {
    ESP_LOGVV(TAG, "  reg[%d]=%02x", i + 1, gp.raw[i]);
  }

  // Let LVGL do gesture recognition
  if (gp.fields.gesture_id != GestureId::GESTURE_NONE) {
    ESP_LOGD(TAG, "Ignoring gesture ID: %02x", gp.fields.gesture_id);
    return;
  }

  uint16_t x, y;
  if (gp.fields.point0.event == EventId::EVENT_CONTACT) {
    x = (gp.fields.point0.x_high << 8) + gp.fields.point0.x_low;
    y = (gp.fields.point0.y_high << 8) + gp.fields.point0.y_low;
    this->add_raw_touch_position_(0, x, y);
    ESP_LOGV(TAG, "Read touch 0: %d/%d", x, y);
  } else {
    // Ignore DOWN and UP
    ESP_LOGD(TAG, "Ignoring point 0 event ID %u", gp.fields.point0.event);
  }

  if ((gp.fields.touch_number == 2) && gp.fields.point1.event == EventId::EVENT_CONTACT) {
    x = (gp.fields.point1.x_high << 8) + gp.fields.point1.x_low;
    y = (gp.fields.point1.y_high << 8) + gp.fields.point1.y_low;
    this->add_raw_touch_position_(1, x, y);
    ESP_LOGV(TAG, "Read touch 1: %d/%d", x, y);
  }
}

void AXS5106Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "AXS5106 Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  ESP_LOGCONFIG(TAG,
                "  X Raw Min: %d, X Raw Max: %d\n"
                "  Y Raw Min: %d, Y Raw Max: %d",
                this->x_raw_min_, this->x_raw_max_, this->y_raw_min_, this->y_raw_max_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace axs5106
}  // namespace esphome
