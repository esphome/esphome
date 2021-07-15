#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace hrxlmaxsonarwr {

class HrxlMaxsonarWrComponent : public sensor::Sensor, public Component, public uart::UARTDevice {
  public:
    // Nothing really public.

    // ========== INTERNAL METHODS ==========
    void loop() override;
    void dump_config() override;

  protected:
    // Nothing.
};

}  // namespace hrxlmaxsonarwr
}  // namespace esphome
