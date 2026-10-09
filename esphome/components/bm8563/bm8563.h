#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome::bm8563 {

class BM8563 final : public time::RealTimeClock, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void write_time();
  void read_time();
  void start_timer(uint32_t duration_s);

 private:
  void get_time_(ESPTime &time);
  void get_date_(ESPTime &time);

  void set_time_(const ESPTime &time);
  void set_date_(const ESPTime &time);

  void set_timer_irq_(uint32_t duration_s);
  void clear_irq_();
  void disable_irq_();

  void write_byte_(uint8_t reg, uint8_t value);
  void write_register_(uint8_t reg, const uint8_t *data, size_t len);
  optional<uint8_t> read_register_(uint8_t reg);

  uint8_t bcd2_to_byte_(uint8_t value);
  uint8_t byte_to_bcd2_(uint8_t value);
};

}  // namespace esphome::bm8563
