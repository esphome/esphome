#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace cst820 {

#define REG_CST820_GE
#define REG_CST820_FINGERINDEX      0x02
#define REG_CST820_XPOSH            0x03
#define REG_CST820_XPOSL            0x04
#define REG_CST820_YPOSH            0x05
#define REG_CST820_YPOSL            0x06
#define REG_CST820_CHIPID           0xA7
#define REG_CST820_PROJECTNUMBER    0xA8
#define REG_CST820_FIRMWAREVERSION  0xA9
#define REG_CST820_IRQPULSEWIDTH    0xED
#define REG_CST820_IRQCONTROL       0xFA
#define REG_CST820_AUTOSLEEP        0xFE

using namespace touchscreen;

class CST820Touchscreen : public Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
 protected:

  void update_touches() override;
};

}  // namespace cst820
}  // namespace esphome
