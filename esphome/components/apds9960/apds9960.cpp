#include "apds9960.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace apds9960 {

static const char *const TAG = "apds9960";

#define APDS9960_ERROR_CHECK(func) \
  if (!(func)) { \
    this->mark_failed(); \
    return; \
  }
#define APDS9960_WRITE_BYTE(reg, value) APDS9960_ERROR_CHECK(this->write_byte(reg, value));

void APDS9960::setup() {
  ESP_LOGCONFIG(TAG, "Setting up APDS9960...");
  uint8_t id;
  if (!this->read_byte(0x92, &id)) {  // ID register
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }

  if (id != 0xAB && id != 0x9C && id != 0xA8) {  // APDS9960 all should have one of these IDs
    this->error_code_ = WRONG_ID;
    this->mark_failed();
    return;
  }

  // ATime (ADC integration time, 2.78ms increments, 0x81) -> 0xDB (103ms)
  APDS9960_WRITE_BYTE(0x81, 0xDB);
  // WTime (Wait time, 0x83) -> 0xF6 (27ms)
  APDS9960_WRITE_BYTE(0x83, 0xF6);
  // PPulse (0x8E) -> 0x87 (16us, 8 pulses)
  APDS9960_WRITE_BYTE(0x8E, 0x87);
  // POffset UR (0x9D) -> 0 (no offset)
  APDS9960_WRITE_BYTE(0x9D, 0x00);
  // POffset DL (0x9E) -> 0 (no offset)
  APDS9960_WRITE_BYTE(0x9E, 0x00);
  // Config 1 (0x8D) -> 0x60 (no wtime factor)
  APDS9960_WRITE_BYTE(0x8D, 0x60);

  // Control (0x8F) ->
  uint8_t val = 0;
  APDS9960_ERROR_CHECK(this->read_byte(0x8F, &val));
  val &= 0b00111111;
  // led drive, 0 -> 100mA, 1 -> 50mA, 2 -> 25mA, 3 -> 12.5mA
  val |= (this->led_drive_ & 0b11) << 6;

  val &= 0b11110011;
  // proximity gain, 0 -> 1x, 1 -> 2X, 2 -> 4X, 3 -> 8X
  val |= (this->proximity_gain_ & 0b11) << 2;

  val &= 0b11111100;
  // ambient light gain, 0 -> 1x, 1 -> 4x, 2 -> 16x, 3 -> 64x
  val |= (this->ambient_gain_ & 0b11) << 0;
  APDS9960_WRITE_BYTE(0x8F, val);

  // Pers (0x8C) -> 0x11 (2 consecutive proximity or ALS for interrupt)
  APDS9960_WRITE_BYTE(0x8C, 0x11);
  // Config 2 (0x90) -> 0x01 (no saturation interrupts or LED boost)
  APDS9960_WRITE_BYTE(0x90, 0x01);
  // Config 3 (0x9F) -> 0x00 (enable all photodiodes, no SAI)
  APDS9960_WRITE_BYTE(0x9F, 0x00);
  // GPenTh (0xA0, gesture enter threshold) -> 0x28 (also 0x32)
  APDS9960_WRITE_BYTE(0xA0, 0x28);
  // GPexTh (0xA1, gesture exit threshold) -> 0x1E
  APDS9960_WRITE_BYTE(0xA1, 0x1E);

  // GConf 1 (0xA2, gesture config 1) -> 0x40 (4 gesture events for interrupt (GFIFO 3), 1 for exit)
  APDS9960_WRITE_BYTE(0xA2, 0x40);

  // GConf 2 (0xA3, gesture config 2) ->
  APDS9960_ERROR_CHECK(this->read_byte(0xA3, &val));
  val &= 0b10011111;
  // gesture gain, 0 -> 1x, 1 -> 2x, 2 -> 4x, 3 -> 8x
  val |= (this->gesture_gain_ & 0b11) << 5;

  val &= 0b11100111;
  // gesture led drive, 0 -> 100mA, 1 -> 50mA, 2 -> 25mA, 3 -> 12.5mA
  val |= (this->gesture_led_drive_ & 0b11) << 3;

  val &= 0b11111000;
  // gesture wait time
  // 0 -> 0ms, 1 -> 2.8ms, 2 -> 5.6ms, 3 -> 8.4ms
  // 4 -> 14.0ms, 5 -> 22.4 ms, 6 -> 30.8ms, 7 -> 39.2 ms
  val |= (this->gesture_wait_time_ & 0b111) << 0;
  APDS9960_WRITE_BYTE(0xA3, val);

  // GOffsetU (0xA4) -> 0x00 (no offset)
  APDS9960_WRITE_BYTE(0xA4, 0x00);
  // GOffsetD (0xA5) -> 0x00 (no offset)
  APDS9960_WRITE_BYTE(0xA5, 0x00);
  // GOffsetL (0xA7) -> 0x00 (no offset)
  APDS9960_WRITE_BYTE(0xA7, 0x00);
  // GOffsetR (0xA9) -> 0x00 (no offset)
  APDS9960_WRITE_BYTE(0xA9, 0x00);
  // GPulse (0xA6) -> 0xC9 (32 µs, 10 pulses)
  APDS9960_WRITE_BYTE(0xA6, 0xC9);

  // GConf 3 (0xAA, gesture config 3) -> 0x00 (all photodiodes active during gesture, all gesture dimensions enabled)
  // 0x00 -> all dimensions, 0x01 -> up down, 0x02 -> left right
  APDS9960_WRITE_BYTE(0xAA, 0x00);

  // Enable (0x80) ->
  val = 0;
  val |= (0b1) << 0;  // power on
  val |= (this->is_color_enabled_() & 0b1) << 1;
  val |= (this->is_proximity_enabled_() & 0b1) << 2;
  val |= 0b0 << 3;                                  // wait timer disabled
  val |= 0b0 << 4;                                  // color interrupt disabled
  val |= 0b0 << 5;                                  // proximity interrupt disabled
  val |= (this->is_gesture_enabled_() & 0b1) << 6;  // proximity is required for gestures
  APDS9960_WRITE_BYTE(0x80, val);
}
bool APDS9960::is_color_enabled_() const {
#ifdef USE_SENSOR
  return this->red_sensor_ != nullptr || this->green_sensor_ != nullptr || this->blue_sensor_ != nullptr ||
         this->clear_sensor_ != nullptr;
#else
  return false;
#endif
}

void APDS9960::dump_config() {
  ESP_LOGCONFIG(TAG, "APDS9960:");
  LOG_I2C_DEVICE(this);

  LOG_UPDATE_INTERVAL(this);

#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Red channel", this->red_sensor_);
  LOG_SENSOR("  ", "Green channel", this->green_sensor_);
  LOG_SENSOR("  ", "Blue channel", this->blue_sensor_);
  LOG_SENSOR("  ", "Clear channel", this->clear_sensor_);
  LOG_SENSOR("  ", "Proximity", this->proximity_sensor_);
#endif

  if (this->is_failed()) {
    switch (this->error_code_) {
      case COMMUNICATION_FAILED:
        ESP_LOGE(TAG, "Communication with APDS9960 failed!");
        break;
      case WRONG_ID:
        ESP_LOGE(TAG, "APDS9960 has invalid id!");
        break;
      default:
        ESP_LOGE(TAG, "Setting up APDS9960 registers failed!");
        break;
    }
  }
}

#define APDS9960_WARNING_CHECK(func, warning) \
  if (!(func)) { \
    ESP_LOGW(TAG, warning); \
    this->status_set_warning(); \
    return; \
  }

void APDS9960::update() {
  uint8_t status;
  APDS9960_WARNING_CHECK(this->read_byte(0x93, &status), "Reading status bit failed.");
  this->status_clear_warning();

  this->read_color_data_(status);
  this->read_proximity_data_(status);
}

void APDS9960::loop() { this->read_gesture_data_(); }

void APDS9960::read_color_data_(uint8_t status) {
  if (!this->is_color_enabled_())
    return;

  if ((status & 0x01) == 0x00) {
    // color data not ready yet.
    return;
  }

  uint8_t raw[8];
  APDS9960_WARNING_CHECK(this->read_bytes(0x94, raw, 8), "Reading color values failed.");

  uint16_t uint_clear = (uint16_t(raw[1]) << 8) | raw[0];
  uint16_t uint_red = (uint16_t(raw[3]) << 8) | raw[2];
  uint16_t uint_green = (uint16_t(raw[5]) << 8) | raw[4];
  uint16_t uint_blue = (uint16_t(raw[7]) << 8) | raw[6];

  float clear_perc = (uint_clear / float(UINT16_MAX)) * 100.0f;
  float red_perc = (uint_red / float(UINT16_MAX)) * 100.0f;
  float green_perc = (uint_green / float(UINT16_MAX)) * 100.0f;
  float blue_perc = (uint_blue / float(UINT16_MAX)) * 100.0f;

  ESP_LOGD(TAG, "Got clear=%.1f%% red=%.1f%% green=%.1f%% blue=%.1f%%", clear_perc, red_perc, green_perc, blue_perc);
#ifdef USE_SENSOR
  if (this->clear_sensor_ != nullptr)
    this->clear_sensor_->publish_state(clear_perc);
  if (this->red_sensor_ != nullptr)
    this->red_sensor_->publish_state(red_perc);
  if (this->green_sensor_ != nullptr)
    this->green_sensor_->publish_state(green_perc);
  if (this->blue_sensor_ != nullptr)
    this->blue_sensor_->publish_state(blue_perc);
#endif
}
void APDS9960::read_proximity_data_(uint8_t status) {
#ifndef USE_SENSOR
  return;
#else
  if (this->proximity_sensor_ == nullptr)
    return;

  if ((status & 0b10) == 0x00) {
    // proximity data not ready yet.
    return;
  }

  uint8_t prox;
  APDS9960_WARNING_CHECK(this->read_byte(0x9C, &prox), "Reading proximity values failed.");

  float prox_perc = (prox / float(UINT8_MAX)) * 100.0f;
  ESP_LOGD(TAG, "Got proximity=%.1f%%", prox_perc);
  this->proximity_sensor_->publish_state(prox_perc);
#endif
}
void APDS9960::read_gesture_data_() {
  if (!this->is_gesture_enabled_())
    return;

  uint8_t status;
  APDS9960_WARNING_CHECK(this->read_byte(0xAF, &status), "Reading gesture status failed.");

  if ((status & 0b01) == 0) {
    // GVALID is false
    return;
  }

  if ((status & 0b10) == 0b10) {
    ESP_LOGV(TAG, "FIFO buffer has filled to capacity!");
  }

  uint8_t fifo_level;
  APDS9960_WARNING_CHECK(this->read_byte(0xAE, &fifo_level), "Reading FIFO level failed.");
  if (fifo_level == 0) {
    // no data to process
    return;
  }

  APDS9960_WARNING_CHECK(fifo_level <= 32, "FIFO level has invalid value.")

  uint8_t buf[128];
  for (uint8_t pos = 0; pos < fifo_level * 4; pos += 32) {
    // The ESP's i2c driver has a limited buffer size.
    // This way of retrieving the data should be wrong according to the datasheet
    // but it seems to work.
    uint8_t read = std::min(32, fifo_level * 4 - pos);
    APDS9960_WARNING_CHECK(this->read_bytes(0xFC + pos, buf + pos, read), "Reading FIFO buffer failed.");
  }

  if (millis() - this->gesture_start_ > 500) {
    this->gesture_up_started_ = false;
    this->gesture_down_started_ = false;
    this->gesture_left_started_ = false;
    this->gesture_right_started_ = false;
  }

  for (uint32_t i = 0; i < fifo_level * 4; i += 4) {
    const int up = buf[i + 0];  // NOLINT
    const int down = buf[i + 1];
    const int left = buf[i + 2];
    const int right = buf[i + 3];
    this->process_dataset_(up, down, left, right);
  }
}
void APDS9960::report_gesture_(int gesture) {
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *bin;
  switch (gesture) {
    case 1:
      bin = this->up_direction_binary_sensor_;
      this->gesture_up_started_ = false;
      this->gesture_down_started_ = false;
      ESP_LOGD(TAG, "Got gesture UP");
      break;
    case 2:
      bin = this->down_direction_binary_sensor_;
      this->gesture_up_started_ = false;
      this->gesture_down_started_ = false;
      ESP_LOGD(TAG, "Got gesture DOWN");
      break;
    case 3:
      bin = this->left_direction_binary_sensor_;
      this->gesture_left_started_ = false;
      this->gesture_right_started_ = false;
      ESP_LOGD(TAG, "Got gesture LEFT");
      break;
    case 4:
      bin = this->right_direction_binary_sensor_;
      this->gesture_left_started_ = false;
      this->gesture_right_started_ = false;
      ESP_LOGD(TAG, "Got gesture RIGHT");
      break;
    default:
      return;
  }

  if (bin != nullptr) {
    bin->publish_state(true);
    bin->publish_state(false);
  }
#endif
}
void APDS9960::process_dataset_(int up, int down, int left, int right) {
  /* Algorithm: (see Figure 11 in datasheet)
   *
   * Observation: When a gesture is started, we will see a short amount of time where
   * the photodiode in the direction of the motion has a much higher count value
   * than where the gesture originates.
   *
   * In this algorithm we continually check the difference between the count values of opposing
   * directions. For example in the down/up direction we continually look at the difference of the
   * up count and down count. When DOWN gesture begins, this difference will be positive with a
   * high magnitude for a short amount of time (magic value here is the difference is at least 13).
   *
   * If we see such a pattern, we store that we saw the first part of a gesture (the leading edge).
   * After that some time can pass during which the difference is zero again (though the count values
   * are not zero). At the end of a gesture, we will see this difference go into the opposite direction
   * for a short period of time.
   *
   * If a gesture is not ended within 500 milliseconds, we consider the initial trailing edge invalid
   * and reset the state.
   *
   * This algorithm does work, but not too well. Some good signal processing algorithms could
   * probably improve this a lot, especially since the incoming signal has such a characteristic
   * and quite noise-free pattern.
   */
  const int up_down_delta = up - down;
  const int left_right_delta = left - right;
  const bool up_down_significant = abs(up_down_delta) > 13;
  const bool left_right_significant = abs(left_right_delta) > 13;

  if (up_down_significant) {
    if (up_down_delta < 0) {
      if (this->gesture_up_started_) {
        // trailing edge of gesture up
        this->report_gesture_(1);  // UP
      } else {
        // leading edge of gesture down
        this->gesture_down_started_ = true;
        this->gesture_start_ = millis();
      }
    } else {
      if (this->gesture_down_started_) {
        // trailing edge of gesture down
        this->report_gesture_(2);  // DOWN
      } else {
        // leading edge of gesture up
        this->gesture_up_started_ = true;
        this->gesture_start_ = millis();
      }
    }
  }

  if (left_right_significant) {
    if (left_right_delta < 0) {
      if (this->gesture_left_started_) {
        // trailing edge of gesture left
        this->report_gesture_(3);  // LEFT
      } else {
        // leading edge of gesture right
        this->gesture_right_started_ = true;
        this->gesture_start_ = millis();
      }
    } else {
      if (this->gesture_right_started_) {
        // trailing edge of gesture right
        this->report_gesture_(4);  // RIGHT
      } else {
        // leading edge of gesture left
        this->gesture_left_started_ = true;
        this->gesture_start_ = millis();
      }
    }
  }
}
float APDS9960::get_setup_priority() const { return setup_priority::DATA; }
bool APDS9960::is_proximity_enabled_() const {
  return
#ifdef USE_SENSOR
      this->proximity_sensor_ != nullptr
#else
      false
#endif
      || this->is_gesture_enabled_();
}
bool APDS9960::is_gesture_enabled_() const {
#ifdef USE_BINARY_SENSOR
  return this->up_direction_binary_sensor_ != nullptr || this->left_direction_binary_sensor_ != nullptr ||
         this->down_direction_binary_sensor_ != nullptr || this->right_direction_binary_sensor_ != nullptr;
#else
  return false;
#endif
}

}  // namespace apds9960
}  // namespace esphome
