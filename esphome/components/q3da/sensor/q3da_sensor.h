#pragma once
#include "esphome/components/q3da/q3da.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace q3da {

class Q3DASensor : public Q3DAListener, public sensor::Sensor, public Component {
 public:
  Q3DASensor(std::string server_id, std::string obis_code);
  void publish_val(const ObisInfo &obis_info) override;
  void dump_config() override;
};

}  // namespace q3da
}  // namespace esphome
