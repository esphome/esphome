#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace bm8563 {

struct BM8563TimeTypeDef {
  int8_t hours;
  int8_t minutes;
  int8_t seconds;
};

struct BM8563DateTypeDef {
  int8_t day;
  int8_t week;
  int8_t month;
  int16_t year;
};

class BM8563 : public time::RealTimeClock, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void write_time();
  void read_time();
  void start_timer(uint32_t timer_s);

 private:
  bool get_volt_low_();

  void get_time_(BM8563TimeTypeDef *b_m8563_time_struct);
  void get_date_(BM8563DateTypeDef *b_m8563_date_struct);

  void set_time_(BM8563TimeTypeDef *b_m8563_time_struct);
  void set_date_(BM8563DateTypeDef *b_m8563_date_struct);

  void set_timer_irq_(uint32_t duration_s);
  void clear_irq_();
  void disable_irq_();

  void write_reg_(uint8_t reg, uint8_t data);
  uint8_t read_reg_(uint8_t reg);

  uint8_t bcd2_to_byte_(uint8_t value);
  uint8_t byte_to_bcd2_(uint8_t value);
};

template<typename... Ts> class WriteAction : public Action<Ts...>, public Parented<BM8563> {
 public:
  void play(Ts... x) override { this->parent_->write_time(); }
};

template<typename... Ts> class ReadAction : public Action<Ts...>, public Parented<BM8563> {
 public:
  void play(Ts... x) override { this->parent_->read_time(); }
};

template<typename... Ts> class TimerAction : public Action<Ts...>, public Parented<BM8563> {
 public:
  TEMPLATABLE_VALUE(uint32_t, duration)

  void play(Ts... x) override {
    auto duration = this->duration_.value(x...);
    this->parent_->start_timer(duration);
  }
};

}  // namespace bm8563
}  // namespace esphome
