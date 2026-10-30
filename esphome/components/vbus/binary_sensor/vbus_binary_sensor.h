#pragma once

#include "../vbus.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome::vbus {

class DeltaSolBSPlusBSensor final : public VBusListener {
 public:
  SUB_BINARY_SENSOR(relay1)
  SUB_BINARY_SENSOR(relay2)
  SUB_BINARY_SENSOR(s1_error)
  SUB_BINARY_SENSOR(s2_error)
  SUB_BINARY_SENSOR(s3_error)
  SUB_BINARY_SENSOR(s4_error)
  SUB_BINARY_SENSOR(collector_max)
  SUB_BINARY_SENSOR(collector_min)
  SUB_BINARY_SENSOR(collector_frost)
  SUB_BINARY_SENSOR(tube_collector)
  SUB_BINARY_SENSOR(recooling)
  SUB_BINARY_SENSOR(hqm)

 protected:
  void handle_message(std::vector<uint8_t> &message) override;
};

class DeltaSolBS2009BSensor final : public VBusListener {
 public:
  SUB_BINARY_SENSOR(s1_error)
  SUB_BINARY_SENSOR(s2_error)
  SUB_BINARY_SENSOR(s3_error)
  SUB_BINARY_SENSOR(s4_error)
  SUB_BINARY_SENSOR(frost_protection_active)

 protected:
  void handle_message(std::vector<uint8_t> &message) override;
};

class DeltaSolCBSensor final : public VBusListener {
 public:
  SUB_BINARY_SENSOR(s1_error)
  SUB_BINARY_SENSOR(s2_error)
  SUB_BINARY_SENSOR(s3_error)
  SUB_BINARY_SENSOR(s4_error)

 protected:
  void handle_message(std::vector<uint8_t> &message) override;
};

class DeltaSolCS2BSensor final : public VBusListener {
 public:
  SUB_BINARY_SENSOR(s1_error)
  SUB_BINARY_SENSOR(s2_error)
  SUB_BINARY_SENSOR(s3_error)
  SUB_BINARY_SENSOR(s4_error)

 protected:
  void handle_message(std::vector<uint8_t> &message) override;
};

class DeltaSolCS4BSensor final : public VBusListener {
 public:
  SUB_BINARY_SENSOR(s1_error)
  SUB_BINARY_SENSOR(s2_error)
  SUB_BINARY_SENSOR(s3_error)
  SUB_BINARY_SENSOR(s4_error)

 protected:
  void handle_message(std::vector<uint8_t> &message) override;
};

class DeltaSolCSPlusBSensor final : public VBusListener {
 public:
  SUB_BINARY_SENSOR(s1_error)
  SUB_BINARY_SENSOR(s2_error)
  SUB_BINARY_SENSOR(s3_error)
  SUB_BINARY_SENSOR(s4_error)

 protected:
  void handle_message(std::vector<uint8_t> &message) override;
};

class DeltaSolBS2BSensor final : public VBusListener {
 public:
  SUB_BINARY_SENSOR(s1_error)
  SUB_BINARY_SENSOR(s2_error)
  SUB_BINARY_SENSOR(s3_error)
  SUB_BINARY_SENSOR(s4_error)

 protected:
  void handle_message(std::vector<uint8_t> &message) override;
};

class VBusCustomSubBSensor;

class VBusCustomBSensor final : public VBusListener {
 public:
  void set_bsensors(std::vector<VBusCustomSubBSensor *> bsensors) { this->bsensors_ = std::move(bsensors); };

 protected:
  std::vector<VBusCustomSubBSensor *> bsensors_;
  void handle_message(std::vector<uint8_t> &message) override;
};

class VBusCustomSubBSensor final : public binary_sensor::BinarySensor {
 public:
  void set_message_parser(message_parser_t parser) { this->message_parser_ = std::move(parser); };
  void parse_message(std::vector<uint8_t> &message);

 protected:
  message_parser_t message_parser_;
};

}  // namespace esphome::vbus
