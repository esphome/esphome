#pragma once

#include "esphome/components/sml/sml.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "../constants.h"

namespace esphome {
namespace sml {

class SmlTextSensor : public SmlListener, public text_sensor::TextSensor, public Component {
 public:
  SmlTextSensor(const std::string& server_id, const std::string& obis_code, SmlType format);
  void publish_val(const ObisInfo &obis_info) override;
  void dump_config() override;

 protected:
  SmlType format_;
};

}  // namespace sml
}  // namespace esphome
