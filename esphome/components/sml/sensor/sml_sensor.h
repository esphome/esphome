#pragma once
#include "esphome/components/sml/sml.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace sml {

class SmlSensor : public SmlListener, public sensor::Sensor, public Component {
 public:
  SmlSensor(const char *server_id, const char *obis);
  void publish_val(ObisInfo obis_info) override;
  void dump_config() override;
};

}  // namespace sml
}  // namespace esphome
