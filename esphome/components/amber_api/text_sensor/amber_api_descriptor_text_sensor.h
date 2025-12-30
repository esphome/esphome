#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "../amber_api.h"

namespace esphome {
namespace amber_api {

class AmberApiDescriptorTextSensor : public text_sensor::TextSensor, public Component, public AmberApiListener {
 public:
  void dump_config() override;
  void on_amber_api_update(const AmberApiData &data) override;
};

}  // namespace amber_api
}  // namespace esphome
