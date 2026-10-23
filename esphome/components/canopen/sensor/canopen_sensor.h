#pragma once

#include "esphome/components/canopen/canopen.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::canopen {

enum class ValueType : uint8_t {
  VALUE_TYPE_UINT8,
  VALUE_TYPE_INT8,
  VALUE_TYPE_UINT16,
  VALUE_TYPE_INT16,
  VALUE_TYPE_UINT32,
  VALUE_TYPE_INT32,
  VALUE_TYPE_FLOAT32,
};

class CANopenSensor : public sensor::Sensor, public PollingComponent {
 public:
  CANopenSensor(CANopenComponent *parent, uint8_t node, uint16_t index, uint8_t subindex, ValueType type)
      : parent_(parent), node_(node), index_(index), subindex_(subindex), type_(type) {}
  void update() override;
  void dump_config() override;

 protected:
  void on_result_(SDOResult result, uint32_t value, uint8_t size);
  CANopenComponent *parent_;
  uint8_t node_;
  uint16_t index_;
  uint8_t subindex_;
  ValueType type_;
  bool pending_{false};
};

}  // namespace esphome::canopen
