#pragma once

#include "esphome/components/mt6701_spi/mt6701_spi.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome::mt6701_spi {

// Reports the magnetic field strength the status bits Mg[1:0] carry
class MT6701SPIFieldStatus : public text_sensor::TextSensor,
                             public PollingComponent,
                             public Parented<MT6701SPIComponent> {
 public:
  void update() override;
  void dump_config() override;
};

}  // namespace esphome::mt6701_spi
