#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace cst820 {

using namespace touchscreen;

struct CST820TouchscreenStore {
  volatile bool touch;
  static void gpio_intr(CST820TouchscreenStore *store);
};

class CST820Touchscreen : public Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  // void update_touches_();
  InternalGPIOPin *irq_pin_{nullptr};
  CST820TouchscreenStore store_;
 protected:
  void reset_();
  bool touched;

  void update_touches() override;
};

}  // namespace cst820
}  // namespace esphome
