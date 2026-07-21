#pragma once

#include "../bq25186.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::bq25186 {

class BQ25186ChargeStatusTextSensor : public BQ25186Listener, public text_sensor::TextSensor {
 public:
  void on_data(const BQ25186Data &data) override {
    const uint8_t status = (data.registers[BQ25186_REG_STAT0] >> 5) & 0x03;
    this->publish_state(this->get_charge_status_string_(status));
  }

 protected:
  const char *get_charge_status_string_(uint8_t status) {
    switch (status) {
      case 0x00:
        return "Not Charging";
      case 0x01:
        return "Constant Current";
      case 0x02:
        return "Constant Voltage";
      case 0x03:
        return "Charge Done/Disabled";
      default:
        return "Unknown";
    }
  }
};

class BQ25186TsStatusTextSensor : public BQ25186Listener, public text_sensor::TextSensor {
 public:
  void on_data(const BQ25186Data &data) override {
    const uint8_t status = (data.registers[BQ25186_REG_STAT1] >> 3) & 0x03;
    this->publish_state(this->get_ts_status_string_(status));
  }

 protected:
  const char *get_ts_status_string_(uint8_t status) {
    switch (status) {
      case 0x00:
        return "Normal";
      case 0x01:
        return "Hot/Cold (Charging Suspended)";
      case 0x02:
        return "Cool (Current Reduced)";
      case 0x03:
        return "Warm (Voltage Reduced)";
      default:
        return "Unknown";
    }
  }
};
}  // namespace esphome::bq25186
