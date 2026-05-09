#pragma once

#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"

#include "../rojaflex.h"

namespace esphome::rojaflex {

enum class RojaflexTextSensorType : uint8_t {
  STATUS,
  CONFIGURED_HOUSECODE,
  LAST_RX_RAW,
  LAST_RX_INFO,
  LAST_TX_ERROR,
  CHANNEL_STATUS,
};

class RojaflexTextSensor : public text_sensor::TextSensor, public PollingComponent, public RojaflexDevice {
 public:
  void set_sensor_type(RojaflexTextSensorType type) { this->type_ = type; }
  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void update() override;

 protected:
  RojaflexTextSensorType type_{RojaflexTextSensorType::STATUS};
  uint8_t channel_{0};
};

}  // namespace esphome::rojaflex
