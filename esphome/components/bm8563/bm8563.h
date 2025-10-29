#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace bm8563 {

struct BM8563_TimeTypeDef {
  int8_t hours;
  int8_t minutes;
  int8_t seconds;
};

struct BM8563_DateTypeDef {
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

  void set_timer_value(uint32_t timer_s);
  void write_time();
  void read_time();
  void start_timer(uint32_t timer_s);

 private:
  bool get_volt_low();

  void get_time(BM8563_TimeTypeDef *BM8563_TimeStruct);
  void get_date(BM8563_DateTypeDef *BM8563_DateStruct);

  void set_time(BM8563_TimeTypeDef *BM8563_TimeStruct);
  void set_date(BM8563_DateTypeDef *BM8563_DateStruct);

  int set_alarm_irq(int duration_s);
  void clear_irq();
  void disable_irq();

  void write_reg(uint8_t reg, uint8_t data);
  uint8_t read_reg(uint8_t reg);

  uint8_t bcd2_to_byte(uint8_t value);
  uint8_t byte_to_bcd2(uint8_t value);

  optional<uint32_t> timer_value_;
  bool setup_complete_;
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
    if (this->duration_.has_value()) {
      auto duration = this->duration_.value(x...);
      this->parent_->start_timer(duration);
    }
  }
};

}  // namespace bm8563
}  // namespace esphome
