#pragma once

#include "esphome/components/q3da/q3da.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "../constants.h"

namespace esphome {
namespace q3da {

class Q3DATextSensor : public Q3DAListener, public text_sensor::TextSensor, public Component {
 public:
  Q3DATextSensor(std::string server_id, std::string obis_code, Q3DAType format);
  void publish_val(const ObisInfo &obis_info) override;
  void dump_config() override;

 protected:
  Q3DAType format_;
};

}  // namespace q3da
}  // namespace esphome
