#pragma once
#include "esphome/core/defines.h"
#ifdef USE_SENSOR
#include "esphome/components/qnetd/qnetd.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome::qnetd {

enum class QnetdSensorType : uint8_t { QNETD_SENSOR_TYPE_CONNECTED_CLIENTS, QNETD_SENSOR_TYPE_DECISIONS };

class QnetdSensor final : public sensor::Sensor, public Component {
 public:
  QnetdSensor(Qnetd *parent, QnetdSensorType type) : parent_(parent), type_(type) {}

  void setup() override;
  void dump_config() override;

 protected:
  void publish_();

  Qnetd *parent_;
  QnetdSensorType type_;
};

}  // namespace esphome::qnetd
#endif  // USE_SENSOR
