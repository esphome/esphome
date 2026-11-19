#pragma once

#include "../vbus.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::vbus {

class DeltaSolBSPlusSensor : public VBusListener, public Component {
 public:
  void dump_config() override;
  SUB_SENSOR(temperature1)
  SUB_SENSOR(temperature2)
  SUB_SENSOR(temperature3)
  SUB_SENSOR(temperature4)
  SUB_SENSOR(pump_speed1)
  SUB_SENSOR(pump_speed2)
  SUB_SENSOR(operating_hours1)
  SUB_SENSOR(operating_hours2)
  SUB_SENSOR(heat_quantity)
  SUB_SENSOR(time)
  SUB_SENSOR(version)

 protected:
  void handle_message(std::vector<uint8_t> &message) override;
};

class DeltaSolBS2009Sensor : public VBusListener, public Component {
 public:
  void dump_config() override;
  SUB_SENSOR(temperature1)
  SUB_SENSOR(temperature2)
  SUB_SENSOR(temperature3)
  SUB_SENSOR(temperature4)
  SUB_SENSOR(pump_speed1)
  SUB_SENSOR(pump_speed2)
  SUB_SENSOR(operating_hours1)
  SUB_SENSOR(operating_hours2)
  SUB_SENSOR(heat_quantity)
  SUB_SENSOR(time)
  SUB_SENSOR(version)

 protected:
  void handle_message(std::vector<uint8_t> &message) override;
};

class DeltaSolCSensor : public VBusListener, public Component {
 public:
  void dump_config() override;
  SUB_SENSOR(temperature1)
  SUB_SENSOR(temperature2)
  SUB_SENSOR(temperature3)
  SUB_SENSOR(temperature4)
  SUB_SENSOR(pump_speed1)
  SUB_SENSOR(pump_speed2)
  SUB_SENSOR(operating_hours1)
  SUB_SENSOR(operating_hours2)
  SUB_SENSOR(heat_quantity)
  SUB_SENSOR(time)

 protected:
  void handle_message(std::vector<uint8_t> &message) override;
};

class DeltaSolCS2Sensor : public VBusListener, public Component {
 public:
  void dump_config() override;
  SUB_SENSOR(temperature1)
  SUB_SENSOR(temperature2)
  SUB_SENSOR(temperature3)
  SUB_SENSOR(temperature4)
  SUB_SENSOR(pump_speed)
  SUB_SENSOR(operating_hours)
  SUB_SENSOR(heat_quantity)
  SUB_SENSOR(version)

 protected:
  void handle_message(std::vector<uint8_t> &message) override;
};

class DeltaSolCS4Sensor : public VBusListener, public Component {
 public:
  void dump_config() override;
  SUB_SENSOR(temperature1)
  SUB_SENSOR(temperature2)
  SUB_SENSOR(temperature3)
  SUB_SENSOR(temperature4)
  SUB_SENSOR(temperature5)
  SUB_SENSOR(pump_speed1)
  SUB_SENSOR(pump_speed2)
  SUB_SENSOR(operating_hours1)
  SUB_SENSOR(operating_hours2)
  SUB_SENSOR(heat_quantity)
  SUB_SENSOR(time)
  SUB_SENSOR(version)
  SUB_SENSOR(flow_rate)

 protected:
  void handle_message(std::vector<uint8_t> &message) override;
};

class DeltaSolCSPlusSensor : public VBusListener, public Component {
 public:
  void dump_config() override;
  SUB_SENSOR(temperature1)
  SUB_SENSOR(temperature2)
  SUB_SENSOR(temperature3)
  SUB_SENSOR(temperature4)
  SUB_SENSOR(temperature5)
  SUB_SENSOR(pump_speed1)
  SUB_SENSOR(pump_speed2)
  SUB_SENSOR(operating_hours1)
  SUB_SENSOR(operating_hours2)
  SUB_SENSOR(heat_quantity)
  SUB_SENSOR(time)
  SUB_SENSOR(version)
  SUB_SENSOR(flow_rate)

 protected:
  void handle_message(std::vector<uint8_t> &message) override;
};

class DeltaSolBS2Sensor : public VBusListener, public Component {
 public:
  void dump_config() override;

  SUB_SENSOR(temperature1)
  SUB_SENSOR(temperature2)
  SUB_SENSOR(temperature3)
  SUB_SENSOR(temperature4)
  SUB_SENSOR(pump_speed1)
  SUB_SENSOR(pump_speed2)
  SUB_SENSOR(operating_hours1)
  SUB_SENSOR(operating_hours2)
  SUB_SENSOR(heat_quantity)
  SUB_SENSOR(version)

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
