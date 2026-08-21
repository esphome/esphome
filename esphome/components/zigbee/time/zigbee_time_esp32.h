#pragma once
#include "esphome/core/defines.h"
#if defined(USE_ZIGBEE) && defined(USE_ESP32) && defined(USE_TIME)

#include "esphome/core/component.h"
#include "esphome/components/time/real_time_clock.h"
#include "../zigbee_esp32.h"

namespace esphome::zigbee {

class ZigbeeComponent;

class ZigbeeTime : public time::RealTimeClock {
 public:
  ZigbeeTime(ZigbeeComponent *parent, uint8_t ep) : endpoint_(ep), parent_(parent) {}
  void setup() override;
  void update() override;
  void dump_config() override;
  void set_epoch_time(uint32_t utc);

 protected:
  void register_zb_time();
  static void set_utc_time(uint32_t utc);
  static uint32_t get_utc_time();
  static void cb(ezb_err_t status);
  uint8_t endpoint_;
  ZigbeeComponent *parent_;
  bool registered_{false};
};

}  // namespace esphome::zigbee
#endif
