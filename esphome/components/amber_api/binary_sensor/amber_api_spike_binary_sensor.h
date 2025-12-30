#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "../amber_api.h"

namespace esphome {
namespace amber_api {

class AmberApiSpikeBinarySensor : public binary_sensor::BinarySensor, public Component, public AmberApiListener {
 public:
  void dump_config() override;
  void on_amber_api_update(const AmberApiData &data) override;
};

}  // namespace amber_api
}  // namespace esphome
