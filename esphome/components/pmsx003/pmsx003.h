#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome::pmsx003 {

enum class Type : uint8_t {
  PMS1003 = 0,
  PMS3003,
  PMSX003,  // PMS5003, PMS6003, PMS7003, PMSA003 (NOT PMSA003I - see `pmsa003i` component)
  PMS5003S,
  PMS5003T,
  PMS5003ST,
  PMS9003M,
  LUFTMY,  // Luftmy LD10, LD11, LD13, LD15, LD16: standard PM + particle counts, no atmospheric PM values
};

enum class Command : uint8_t {
  MEASUREMENT_MODE = 0xE1,  // Data Options: `CMD_MEASUREMENT_MODE_PASSIVE`, `CMD_MEASUREMENT_MODE_ACTIVE`
  MANUAL_MEASUREMENT = 0xE2,
  SLEEP_MODE = 0xE4,  // Data Options: `CMD_SLEEP_MODE_SLEEP`, `CMD_SLEEP_MODE_WAKEUP`
};

enum class State : uint8_t {
  IDLE = 0,
  STABILISING,
  WAITING,
};

class PMSX003Component final : public uart::UARTDevice, public Component {
 public:
  PMSX003Component() = default;
  void setup() override;
  void dump_config() override;
  void loop() override;

  void set_update_interval(uint32_t update_interval) { this->update_interval_ = update_interval; }

  void set_type(Type type) { this->type_ = type; }

  // "Standard Particle"
  SUB_SENSOR(pm_1_0_std)
  SUB_SENSOR(pm_2_5_std)
  SUB_SENSOR(pm_10_0_std)

  // "Under Atmospheric Pressure"
  SUB_SENSOR(pm_1_0)
  SUB_SENSOR(pm_2_5)
  SUB_SENSOR(pm_10_0)

  SUB_SENSOR(pm_particles_03um)
  SUB_SENSOR(pm_particles_05um)
  SUB_SENSOR(pm_particles_10um)
  SUB_SENSOR(pm_particles_25um)
  SUB_SENSOR(pm_particles_50um)
  SUB_SENSOR(pm_particles_100um)

  SUB_SENSOR(formaldehyde)

  SUB_SENSOR(temperature)
  SUB_SENSOR(humidity)

 protected:
  optional<bool> check_byte_();
  void parse_data_();
  bool check_payload_length_(uint16_t payload_length);
  void send_command_(Command cmd, uint16_t data);
  uint16_t get_16_bit_uint_(uint8_t start_index) const {
    return encode_uint16(this->data_[start_index], this->data_[start_index + 1]);
  }

  Type type_;
  State state_{State::IDLE};
  bool initialised_{false};
  uint8_t data_[64];
  uint8_t data_index_{0};
  uint32_t fan_on_time_{0};
  uint32_t last_update_{0};
  uint32_t last_transmission_{0};
  uint32_t update_interval_{0};
};

}  // namespace esphome::pmsx003
