#pragma once

#include <cinttypes>
#include <vector>

#include "esphome/components/ble_device_base/ble_device.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome::mopeka_std_check {

enum SensorType {
  STANDARD = 0x02,
  XL = 0x03,
  STANDARD_ALT = 0x44,
  ETRAILER = 0x46,
};

// 4 values in one struct so it aligns to 8 byte. One `mopeka_std_values` is 40 bit long.
struct mopeka_std_values {  // NOLINT(readability-identifier-naming,altera-struct-pack-align)
  u_int16_t time_0 : 5;
  u_int16_t value_0 : 5;
  u_int16_t time_1 : 5;
  u_int16_t value_1 : 5;
  u_int16_t time_2 : 5;
  u_int16_t value_2 : 5;
  u_int16_t time_3 : 5;
  u_int16_t value_3 : 5;
} __attribute__((packed));

struct mopeka_std_package {  // NOLINT(readability-identifier-naming,altera-struct-pack-align)
  u_int8_t data_0 : 8;
  u_int8_t data_1 : 8;
  u_int8_t raw_voltage : 8;

  u_int8_t raw_temp : 6;
  bool slow_update_rate : 1;
  bool sync_pressed : 1;

  mopeka_std_values val[3];
} __attribute__((packed));

class MopekaStdCheck final : public Component, public ble_device_base::ESPBTDeviceListener {
  SUB_SENSOR(level)
  SUB_SENSOR(temperature)
  SUB_SENSOR(battery_level)
  SUB_SENSOR(distance)

 public:
  void set_address(uint64_t address) { address_ = address; };

  bool parse_device(const ble_device_base::ESPBTDevice &device) override;
  void dump_config() override;

  void set_propane_butane_mix(float val) { this->propane_butane_mix_ = val; };
  void set_tank_full(float full) { this->full_mm_ = full; };
  void set_tank_empty(float empty) { this->empty_mm_ = empty; };

 protected:
  uint64_t address_;

  float propane_butane_mix_;
  uint32_t full_mm_;
  uint32_t empty_mm_;

  float get_lpg_speed_of_sound_(float temperature);
  uint8_t parse_battery_level_(const mopeka_std_package *message);
  int8_t parse_temperature_(const mopeka_std_package *message);
};

}  // namespace esphome::mopeka_std_check
