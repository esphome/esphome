#pragma once

#include "../bq25186.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome::bq25186 {

template<uint8_t REG, uint8_t SHIFT, uint8_t MASK, uint8_t TRUE_VALUE>
class StatusBinarySensor : public BQ25186Listener, public binary_sensor::BinarySensor {
 public:
  void on_data(const BQ25186Data &data) override {
    const uint8_t value = (data.registers[REG] >> SHIFT) & MASK;
    this->publish_state(value == TRUE_VALUE);
  }
};

class BQ25186ChargingActiveBinarySensor : public BQ25186Listener, public binary_sensor::BinarySensor {
 public:
  void on_data(const BQ25186Data &data) override {
    const uint8_t charge_status = (data.registers[BQ25186_REG_STAT0] >> 5) & 0x03;
    this->publish_state(charge_status == 0x01 || charge_status == 0x02);
  }
};

using BQ25186ChargeDoneBinarySensor = StatusBinarySensor<BQ25186_REG_STAT0, 5, 0x03, 0x03>;
using BQ25186VinPowerGoodBinarySensor = StatusBinarySensor<BQ25186_REG_STAT0, 0, 0x01, 0x01>;
using BQ25186IlimActiveBinarySensor = StatusBinarySensor<BQ25186_REG_STAT0, 4, 0x01, 0x01>;
using BQ25186VdppmActiveBinarySensor = StatusBinarySensor<BQ25186_REG_STAT0, 3, 0x01, 0x01>;
using BQ25186VindpmActiveBinarySensor = StatusBinarySensor<BQ25186_REG_STAT0, 2, 0x01, 0x01>;
using BQ25186ThermregActiveBinarySensor = StatusBinarySensor<BQ25186_REG_STAT0, 1, 0x01, 0x01>;
using BQ25186VinOvpActiveBinarySensor = StatusBinarySensor<BQ25186_REG_STAT1, 7, 0x01, 0x01>;
using BQ25186BatteryUvloActiveBinarySensor = StatusBinarySensor<BQ25186_REG_STAT1, 6, 0x01, 0x01>;
using BQ25186SafetyTimerFaultBinarySensor = StatusBinarySensor<BQ25186_REG_STAT1, 2, 0x01, 0x01>;
using BQ25186Wake1FlagBinarySensor = StatusBinarySensor<BQ25186_REG_STAT1, 1, 0x01, 0x01>;
using BQ25186Wake2FlagBinarySensor = StatusBinarySensor<BQ25186_REG_STAT1, 0, 0x01, 0x01>;
using BQ25186TsOpenBinarySensor = StatusBinarySensor<BQ25186_REG_STAT0, 7, 0x01, 0x01>;
using BQ25186TsFaultBinarySensor = StatusBinarySensor<BQ25186_REG_FLAG0, 7, 0x01, 0x01>;
using BQ25186BatteryOcpFaultBinarySensor = StatusBinarySensor<BQ25186_REG_FLAG0, 0, 0x01, 0x01>;

}  // namespace esphome::bq25186
