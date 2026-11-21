#pragma once

#include "../vbus.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/log.h"

namespace esphome::vbus {

// Values shared by most DeltaSol controllers. A model only wires up the fields it actually has;
// the rest stay null and are skipped when publishing and logging.
class DeltaSolSensorBase : public VBusListener, public Component {
 public:
  SUB_SENSOR(temperature1)
  SUB_SENSOR(temperature2)
  SUB_SENSOR(temperature3)
  SUB_SENSOR(temperature4)
  SUB_SENSOR(pump_speed1)
  SUB_SENSOR(pump_speed2)
  SUB_SENSOR(operating_hours1)
  SUB_SENSOR(operating_hours2)
  // Single relay models (CS2) use the unnumbered names of the first pump / hour counter.
  void set_pump_speed_sensor(sensor::Sensor *sensor) { this->pump_speed1_sensor_ = sensor; }
  void set_operating_hours_sensor(sensor::Sensor *sensor) { this->operating_hours1_sensor_ = sensor; }
  SUB_SENSOR(heat_quantity)
  SUB_SENSOR(time)
  SUB_SENSOR(version)

 protected:
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
  // Only the CS4 family has these fields.
  SUB_SENSOR(temperature5)
  SUB_SENSOR(flow_rate)

 protected:
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

class VBusCustomSubSensor : public sensor::Sensor {
 public:
  void set_message_parser(message_parser_t parser) { this->message_parser_ = std::move(parser); };
  void parse_message(std::vector<uint8_t> &message);

 protected:
  message_parser_t message_parser_;
};

}  // namespace esphome::vbus
