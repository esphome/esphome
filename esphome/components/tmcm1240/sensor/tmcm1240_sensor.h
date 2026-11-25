#pragma once

#include "esphome/components/tmcm1240/tmcm1240.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::tmcm1240 {

class TMCM1240Sensor : public sensor::Sensor, public PollingComponent {
 public:
  TMCM1240Sensor(TMCM1240 *parent, uint16_t index, uint8_t size) : parent_(parent), index_(index), size_(size) {}
  void update() override;

 protected:
  static void on_result(void *context, const canopen::SDOReply &reply);
  TMCM1240 *parent_;
  uint16_t index_;
  uint8_t size_;
  bool pending_{false};
};

}  // namespace esphome::tmcm1240
