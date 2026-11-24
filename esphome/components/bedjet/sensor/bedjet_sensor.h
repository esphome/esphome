#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/bedjet/bedjet_child.h"
#include "esphome/components/bedjet/bedjet_codec.h"

namespace esphome::bedjet {

class BedjetSensor final : public BedJetClient {
  SUB_SENSOR(outlet_temperature)
  SUB_SENSOR(ambient_temperature)

 public:
  void dump_child_config() override;

  void on_status(const BedjetStatusPacket *data) override;
  void on_bedjet_state(bool is_ready) override;
  std::string describe() override;
};

}  // namespace esphome::bedjet
