#pragma once

#include "esphome/core/component.h"
#include "esphome/components/cover/cover.h"
#include "esphome/components/rs485/rs485.h"

namespace esphome {
namespace dooya {

static const uint8_t START_CODE = 0x55;

enum : uint8_t

class Dooya : public cover::Cover, public Component, public rs485::RS485Device {
 public:
  void setup() override;
  void dump_config() override;

  void set_address(uint16_t address) {
    this->address_[0] = (uint8_t)(address >> 8);
    this->address_[1] = (uint8_t)(address & 0xFF);
  }

  void on_rs485_data(const std::vector<uint8_t> &data) override;

  cover::CoverTraits get_traits() override;

 protected:
  uint8_t address_[2] = {0xFE, 0xFE};

}  // namespace pzemac
}  // namespace esphome
