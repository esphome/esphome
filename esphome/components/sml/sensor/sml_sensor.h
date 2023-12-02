#pragma once
#include "esphome/components/sml/sml.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace sml {

class SmlSensor : public SmlListener, public sensor::Sensor, public Component {
 public:
  SmlSensor(std::string server_id, std::string obis_code);
  void publish_val(const ObisInfo &obis_info) override;
  void dump_config() override;
};

}  // namespace sml
}  // namespace esphome
