#pragma once

#include "../vbus.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace vbus {

class DeltaSolBSPlusSensor : public VBusListener, public Component {
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

  void handle_message(std::vector<uint8_t> &message) override;
};

class DeltaSolCSensor : public VBusListener, public Component {
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

  void handle_message(std::vector<uint8_t> &message) override;
};

class DeltaSolCS2Sensor : public VBusListener, public Component {
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
  sensor::Sensor *temperature1_sensor_{nullptr};
  sensor::Sensor *temperature2_sensor_{nullptr};
  sensor::Sensor *temperature3_sensor_{nullptr};
  sensor::Sensor *temperature4_sensor_{nullptr};
  sensor::Sensor *pump_speed_sensor_{nullptr};
  sensor::Sensor *operating_hours_sensor_{nullptr};
  sensor::Sensor *heat_quantity_sensor_{nullptr};
  sensor::Sensor *version_sensor_{nullptr};

  void handle_message(std::vector<uint8_t> &message) override;
};

class DeltaSolCSPlusSensor : public VBusListener, public Component {
 public:
  void dump_config() override;
  void set_temperature1_sensor(sensor::Sensor *sensor) { this->temperature1_sensor_ = sensor; }
  void set_temperature2_sensor(sensor::Sensor *sensor) { this->temperature2_sensor_ = sensor; }
  void set_temperature3_sensor(sensor::Sensor *sensor) { this->temperature3_sensor_ = sensor; }
  void set_temperature4_sensor(sensor::Sensor *sensor) { this->temperature4_sensor_ = sensor; }
  void set_temperature5_sensor(sensor::Sensor *sensor) { this->temperature5_sensor_ = sensor; }
  void set_pump_speed1_sensor(sensor::Sensor *sensor) { this->pump_speed1_sensor_ = sensor; }
  void set_pump_speed2_sensor(sensor::Sensor *sensor) { this->pump_speed2_sensor_ = sensor; }
  void set_operating_hours1_sensor(sensor::Sensor *sensor) { this->operating_hours1_sensor_ = sensor; }
  void set_operating_hours2_sensor(sensor::Sensor *sensor) { this->operating_hours2_sensor_ = sensor; }
  void set_heat_quantity_sensor(sensor::Sensor *sensor) { this->heat_quantity_sensor_ = sensor; }
  void set_time_sensor(sensor::Sensor *sensor) { this->time_sensor_ = sensor; }
  void set_version_sensor(sensor::Sensor *sensor) { this->version_sensor_ = sensor; }
  void set_flow_rate_sensor(sensor::Sensor *sensor) { this->flow_rate_sensor_ = sensor; }

 protected:
  sensor::Sensor *temperature1_sensor_{nullptr};
  sensor::Sensor *temperature2_sensor_{nullptr};
  sensor::Sensor *temperature3_sensor_{nullptr};
  sensor::Sensor *temperature4_sensor_{nullptr};
  sensor::Sensor *temperature5_sensor_{nullptr};
  sensor::Sensor *pump_speed1_sensor_{nullptr};
  sensor::Sensor *pump_speed2_sensor_{nullptr};
  sensor::Sensor *operating_hours1_sensor_{nullptr};
  sensor::Sensor *operating_hours2_sensor_{nullptr};
  sensor::Sensor *heat_quantity_sensor_{nullptr};
  sensor::Sensor *time_sensor_{nullptr};
  sensor::Sensor *version_sensor_{nullptr};
  sensor::Sensor *flow_rate_sensor_{nullptr};

  void handle_message(std::vector<uint8_t> &message) override;
};

class VBusCustomSubSensor;

class VBusCustomSensor : public VBusListener, public Component {
 public:
  void dump_config() override;
  void set_sensors(std::vector<VBusCustomSubSensor *> sensors) { this->sensors_ = std::move(sensors); };

 protected:
  std::vector<VBusCustomSubSensor *> sensors_;
  void handle_message(std::vector<uint8_t> &message) override;
};

class VBusCustomSubSensor : public sensor::Sensor, public Component {
 public:
  void set_message_parser(message_parser_t parser) { this->message_parser_ = std::move(parser); };
  void parse_message(std::vector<uint8_t> &message);

 protected:
  message_parser_t message_parser_;
};

}  // namespace vbus
}  // namespace esphome
