#pragma once

#include "esphome/components/micronova/micronova.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace micronova {

class MicroNovaNumber : public number::Number, public MicroNovaSensorListener {
 public:
  MicroNovaNumber() {}
  MicroNovaNumber(MicroNova *m) : MicroNovaSensorListener(m) {}
  void dump_config() override { LOG_NUMBER("", "Micronova number", this); }
  void control(float value) override;
  void request_value_from_stove() override {
    this->micronova_->request_address(this->memory_location_, this->memory_address_, this);
  }
  void process_value_from_stove(int value_from_stove) override;

  void set_memory_write_location(uint8_t l) { this->memory_write_location_ = l; }
  uint8_t get_memory_write_location() { return this->memory_write_location_; }

 protected:
  uint8_t memory_write_location_ = 0;
};

}  // namespace micronova
}  // namespace esphome
