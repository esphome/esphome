#pragma once

#include "../vbus.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/log.h"

namespace esphome::vbus {

// Values shared by most DeltaSol controllers. A model only wires up the fields it actually has;
// the rest stay null and are skipped when publishing and logging.
class DeltaSolSensorBase : public VBusListener, public Component {
 public:
  void set_temperature1_sensor(sensor::Sensor *sensor) { this->temperature1_sensor_ = sensor; }
  void set_temperature2_sensor(sensor::Sensor *sensor) { this->temperature2_sensor_ = sensor; }
  void set_temperature3_sensor(sensor::Sensor *sensor) { this->temperature3_sensor_ = sensor; }
  void set_temperature4_sensor(sensor::Sensor *sensor) { this->temperature4_sensor_ = sensor; }
  void set_pump_speed1_sensor(sensor::Sensor *sensor) { this->pump_speed1_sensor_ = sensor; }
  void set_pump_speed2_sensor(sensor::Sensor *sensor) { this->pump_speed2_sensor_ = sensor; }
  void set_operating_hours1_sensor(sensor::Sensor *sensor) { this->operating_hours1_sensor_ = sensor; }
  void set_operating_hours2_sensor(sensor::Sensor *sensor) { this->operating_hours2_sensor_ = sensor; }
  // Single relay models (CS2) use the unnumbered names of the first pump / hour counter.
  void set_pump_speed_sensor(sensor::Sensor *sensor) { this->pump_speed1_sensor_ = sensor; }
  void set_operating_hours_sensor(sensor::Sensor *sensor) { this->operating_hours1_sensor_ = sensor; }
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
  // Latches so a mismatched model/truncated telegram warns once instead of on every telegram (~1/s).
  bool warned_short_message_{false};

  // Single relay models label the first pump / hour counter without a number, matching their config keys.
  void dump_sensors_(const LogString *model, bool single_relay = false);
  void warn_short_message_(size_t actual, size_t expected);
};

class DeltaSolBSPlusSensor final : public DeltaSolSensorBase {
 public:
  void dump_config() override { this->dump_sensors_(LOG_STR("Deltasol BS Plus")); }

 protected:
  void handle_message(std::vector<uint8_t> &message) override;
};

class DeltaSolBS2009Sensor final : public DeltaSolSensorBase {
 public:
  void dump_config() override { this->dump_sensors_(LOG_STR("Deltasol BS 2009")); }

 protected:
  void handle_message(std::vector<uint8_t> &message) override;
};

class DeltaSolCSensor final : public DeltaSolSensorBase {
 public:
  void dump_config() override { this->dump_sensors_(LOG_STR("Deltasol C")); }

 protected:
  void handle_message(std::vector<uint8_t> &message) override;
};

class DeltaSolCS2Sensor final : public DeltaSolSensorBase {
 public:
  void dump_config() override { this->dump_sensors_(LOG_STR("Deltasol CS2"), true); }

 protected:
  void handle_message(std::vector<uint8_t> &message) override;
};

class DeltaSolCS4Sensor : public DeltaSolSensorBase {
 public:
  void dump_config() override { this->dump_cs4_sensors_(LOG_STR("Deltasol CS4")); }
  void set_temperature5_sensor(sensor::Sensor *sensor) { this->temperature5_sensor_ = sensor; }
  void set_flow_rate_sensor(sensor::Sensor *sensor) { this->flow_rate_sensor_ = sensor; }

 protected:
  // Only the CS4 family has these, so they are kept out of the base to save RAM on the other models.
  sensor::Sensor *temperature5_sensor_{nullptr};
  sensor::Sensor *flow_rate_sensor_{nullptr};

  void dump_cs4_sensors_(const LogString *model);
  void handle_message(std::vector<uint8_t> &message) override;
};

// The CS Plus reports the same payload layout as the CS4.
class DeltaSolCSPlusSensor final : public DeltaSolCS4Sensor {
 public:
  void dump_config() override { this->dump_cs4_sensors_(LOG_STR("Deltasol CS Plus")); }
};

class DeltaSolBS2Sensor final : public DeltaSolSensorBase {
 public:
  void dump_config() override { this->dump_sensors_(LOG_STR("DeltaSol BS/2 (DrainBack)")); }

 protected:
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

}  // namespace esphome::vbus
