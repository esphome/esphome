#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "tuya.h"

#include <vector>

namespace esphome {
namespace tuya {

class TuyaDatapointUpdateTrigger : public Trigger<TuyaDatapoint> {
 public:
  explicit TuyaDatapointUpdateTrigger(Tuya *parent, uint8_t sensor_id) {
    parent->register_listener(sensor_id, [this](const TuyaDatapoint &dp) { this->trigger(dp); });
  }
};

class TuyaRawDatapointUpdateTrigger : public Trigger<std::vector<uint8_t>> {
 public:
  explicit TuyaRawDatapointUpdateTrigger(Tuya *parent, uint8_t sensor_id);
};

class TuyaBoolDatapointUpdateTrigger : public Trigger<bool> {
 public:
  explicit TuyaBoolDatapointUpdateTrigger(Tuya *parent, uint8_t sensor_id);
};

class TuyaIntDatapointUpdateTrigger : public Trigger<int> {
 public:
  explicit TuyaIntDatapointUpdateTrigger(Tuya *parent, uint8_t sensor_id);
};

class TuyaUIntDatapointUpdateTrigger : public Trigger<uint32_t> {
 public:
  explicit TuyaUIntDatapointUpdateTrigger(Tuya *parent, uint8_t sensor_id);
};

class TuyaStringDatapointUpdateTrigger : public Trigger<std::string> {
 public:
  explicit TuyaStringDatapointUpdateTrigger(Tuya *parent, uint8_t sensor_id);
};

class TuyaEnumDatapointUpdateTrigger : public Trigger<uint8_t> {
 public:
  explicit TuyaEnumDatapointUpdateTrigger(Tuya *parent, uint8_t sensor_id);
};

class TuyaBitmaskDatapointUpdateTrigger : public Trigger<uint32_t> {
 public:
  explicit TuyaBitmaskDatapointUpdateTrigger(Tuya *parent, uint8_t sensor_id);
};

}  // namespace tuya
}  // namespace esphome
