#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace tt21100 {

using namespace touchscreen;

static const uint8_t MAX_BUTTONS = 4;
static const uint8_t MAX_TOUCH_POINTS = 5;
static const uint8_t MAX_DATA_LEN = (7 + MAX_TOUCH_POINTS * 10);  // 7 Header + (Points * 10 data bytes)

struct TT21100TouchscreenStore {
  volatile bool touch;
  ISRInternalGPIOPin pin;

  static void gpio_intr(TT21100TouchscreenStore *store);
};

class TT21100Touchscreen : public Touchscreen, public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }
  void set_button(int index, binary_sensor::BinarySensor *sensor) { this->buttons_[index] = sensor; }

 protected:
  void reset_();

  InternalGPIOPin *interrupt_pin_;
  GPIOPin *reset_pin_{nullptr};
  std::array<binary_sensor::BinarySensor *, MAX_BUTTONS> buttons_;

  TT21100TouchscreenStore store_;
};

struct TT21100ButtonReport {
  uint16_t length;    /*!< Always 14(0x000E) */
  uint8_t report_id;  /*!< Always 0x03 */
  uint16_t timestamp; /*!< Number in units of 100 us */
  uint8_t btn_val;    /*!< Only use bit[0..3] */
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
  uint8_t noise_efect : 3;
  TT21100TouchRecord touch_record[0];
} __attribute__((packed));

}  // namespace tt21100
}  // namespace esphome
