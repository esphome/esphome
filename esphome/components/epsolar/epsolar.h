#pragma once

#include "modbuscomponent.h"

namespace esphome {
namespace epsolar {

class EPSOLAR : public ModbusComponent {
 public:
  void update() override;
  void setup() override;
  void set_realtime_clock_to_now();
  void dump_config() override;

  void set_sync_rtc(bool do_sync) { sync_rtc_ = do_sync; }

 protected:
  sensor::Sensor *light_signal_turn_off_delay_time_sensor_;
  bool sync_rtc_;
};

}  // namespace epsolar
}  // namespace esphome
