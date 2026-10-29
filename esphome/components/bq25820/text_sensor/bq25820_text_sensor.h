#pragma once

#include "../bq25820.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::bq25820 {

// Charge cycle status text, decoded from CHARGE_STAT[2:0] (datasheet Table 7-29).
// Index 5 is a reserved code.
class BQ25820ChargeStatusTextSensor : public BQ25820Listener, public text_sensor::TextSensor {
 public:
  void on_data(const BQ25820Data &data) override {
    static const char *const STATE_STRINGS[] = {
        "Not Charging",  // 0
        "Trickle",       // 1
        "Precharge",     // 2
        "Fast Charge",   // 3
        "Taper",         // 4
        "Unknown",       // 5 (reserved)
        "Top-Off",       // 6
        "Done",          // 7
    };
    uint8_t idx = data.charge_stat;
    this->publish_state(idx < 8 ? STATE_STRINGS[idx] : "Unknown");
  }
};

// Battery NTC temperature status, decoded from TS_STAT[2:0] (datasheet Table 7-30).
// Codes 5-7 are not defined by the device.
class BQ25820TempStatusTextSensor : public BQ25820Listener, public text_sensor::TextSensor {
 public:
  void on_data(const BQ25820Data &data) override {
    static const char *const STATE_STRINGS[] = {
        "Normal",  // 0
        "Warm",    // 1
        "Cool",    // 2
        "Cold",    // 3
        "Hot",     // 4
        "Unknown"  // 5-7 (not defined)
    };
    uint8_t idx = data.battery_temp_stat;
    this->publish_state(idx < 5 ? STATE_STRINGS[idx] : STATE_STRINGS[5]);
  }
};

}  // namespace esphome::bq25820
