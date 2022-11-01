#pragma once

#include "../vbus.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace vbus {

using message_handler_t = std::function<void(std::vector<uint8_t> &)>;

class DeltaSol_C_sensor : public VBusListener, public Component {
 public:
  void dump_config() override;
  void set_temperature1_sensor(sensor::Sensor *sensor) { this->temperature1_sensor_ = sensor; }
  void set_temperature2_sensor(sensor::Sensor *sensor) { this->temperature2_sensor_ = sensor; }
  void set_temperature3_sensor(sensor::Sensor *sensor) { this->temperature3_sensor_ = sensor; }
  void set_temperature4_sensor(sensor::Sensor *sensor) { this->temperature4_sensor_ = sensor; }
  void set_pump_speed1_sensor(sensor::Sensor *sensor) { this->pump_speed1_sensor_ = sensor; }
  void set_pump_speed2_sensor(sensor::Sensor *sensor) { this->pump_speed2_sensor_ = sensor; }
  void set_operating_hours1_sensor(sensor::Sensor *sensor) { this->operating_hours1_sensor_ = sensor; }
  void set_operating_hours2_sensor(sensor::Sensor *sensor) { this->operating_hours2_sensor_ = sensor; }
  void set_heat_quantity_sensor(sensor::Sensor *sensor) { this->heat_quantity_sensor_ = sensor; }
  void set_time_sensor(sensor::Sensor *sensor) { this->time_sensor_ = sensor; }

 protected:
  static constexpr const char *TAG_ = "vbus.deltasol_c.sensor";
  sensor::Sensor *temperature1_sensor_{nullptr};
  sensor::Sensor *temperature2_sensor_{nullptr};
  sensor::Sensor *temperature3_sensor_{nullptr};
  sensor::Sensor *temperature4_sensor_{nullptr};
  sensor::Sensor *pump_speed1_sensor_{nullptr};
  sensor::Sensor *pump_speed2_sensor_{nullptr};
  sensor::Sensor *operating_hours1_sensor_{nullptr};
  sensor::Sensor *operating_hours2_sensor_{nullptr};
  sensor::Sensor *heat_quantity_sensor_{nullptr};
  sensor::Sensor *time_sensor_{nullptr};

  void handle_message_(std::vector<uint8_t> &message) override;
};

class DeltaSol_CS2_sensor : public VBusListener, public Component {
 public:
  void dump_config() override;
  void set_temperature1_sensor(sensor::Sensor *sensor) { this->temperature1_sensor_ = sensor; }
  void set_temperature2_sensor(sensor::Sensor *sensor) { this->temperature2_sensor_ = sensor; }
  void set_temperature3_sensor(sensor::Sensor *sensor) { this->temperature3_sensor_ = sensor; }
  void set_temperature4_sensor(sensor::Sensor *sensor) { this->temperature4_sensor_ = sensor; }
  void set_pump_speed_sensor(sensor::Sensor *sensor) { this->pump_speed_sensor_ = sensor; }
  void set_operating_hours_sensor(sensor::Sensor *sensor) { this->operating_hours_sensor_ = sensor; }
  void set_heat_quantity_sensor(sensor::Sensor *sensor) { this->heat_quantity_sensor_ = sensor; }
  void set_version_sensor(sensor::Sensor *sensor) { this->version_sensor_ = sensor; }

 protected:
  static constexpr const char *TAG_ = "vbus.deltasol_cs2.sensor";
  sensor::Sensor *temperature1_sensor_{nullptr};
  sensor::Sensor *temperature2_sensor_{nullptr};
  sensor::Sensor *temperature3_sensor_{nullptr};
  sensor::Sensor *temperature4_sensor_{nullptr};
  sensor::Sensor *pump_speed_sensor_{nullptr};
  sensor::Sensor *operating_hours_sensor_{nullptr};
  sensor::Sensor *heat_quantity_sensor_{nullptr};
  sensor::Sensor *version_sensor_{nullptr};

  void handle_message_(std::vector<uint8_t> &message) override;
};

class DeltaSol_BS_Plus_sensor : public VBusListener, public Component {
 public:
  void dump_config() override;
  void set_temperature1_sensor(sensor::Sensor *sensor) { this->temperature1_sensor_ = sensor; }
  void set_temperature2_sensor(sensor::Sensor *sensor) { this->temperature2_sensor_ = sensor; }
  void set_temperature3_sensor(sensor::Sensor *sensor) { this->temperature3_sensor_ = sensor; }
  void set_temperature4_sensor(sensor::Sensor *sensor) { this->temperature4_sensor_ = sensor; }
  void set_pump_speed1_sensor(sensor::Sensor *sensor) { this->pump_speed1_sensor_ = sensor; }
  void set_pump_speed2_sensor(sensor::Sensor *sensor) { this->pump_speed2_sensor_ = sensor; }
  void set_operating_hours1_sensor(sensor::Sensor *sensor) { this->operating_hours1_sensor_ = sensor; }
  void set_operating_hours2_sensor(sensor::Sensor *sensor) { this->operating_hours2_sensor_ = sensor; }
  void set_heat_quantity_sensor(sensor::Sensor *sensor) { this->heat_quantity_sensor_ = sensor; }
  void set_time_sensor(sensor::Sensor *sensor) { this->time_sensor_ = sensor; }
  void set_version_sensor(sensor::Sensor *sensor) { this->version_sensor_ = sensor; }

 protected:
  static constexpr const char *TAG_ = "vbus.deltasol_bs_plus.sensor";
  sensor::Sensor *temperature1_sensor_{nullptr};
  sensor::Sensor *temperature2_sensor_{nullptr};
  sensor::Sensor *temperature3_sensor_{nullptr};
  sensor::Sensor *temperature4_sensor_{nullptr};
  sensor::Sensor *pump_speed1_sensor_{nullptr};
  sensor::Sensor *pump_speed2_sensor_{nullptr};
  sensor::Sensor *operating_hours1_sensor_{nullptr};
  sensor::Sensor *operating_hours2_sensor_{nullptr};
  sensor::Sensor *heat_quantity_sensor_{nullptr};
  sensor::Sensor *time_sensor_{nullptr};
  sensor::Sensor *version_sensor_{nullptr};

  void handle_message_(std::vector<uint8_t> &message) override;
};

class VBusCustom_sensor : public VBusListener, public Component {
 public:
  void dump_config() override;
  void set_message_handler(message_handler_t &&handler) { this->message_handler_ = handler; };
 protected:
  static constexpr const char *TAG_ = "vbus.custom";
  optional<message_handler_t> message_handler_{};
  void handle_message_(std::vector<uint8_t> &message) override;
};

}  // namespace vbus
}  // namespace esphome
