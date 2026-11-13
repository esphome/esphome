#pragma once

#include "../vbus.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome::vbus {

// Every supported model reports the four sensor failure flags as one bitmask byte with the same
// bit order; only the offset of that byte within the payload differs per model.
class DeltaSolErrorsBSensorBase : public VBusListener, public Component {
 public:
  SUB_BINARY_SENSOR(s1_error)
  SUB_BINARY_SENSOR(s2_error)
  SUB_BINARY_SENSOR(s3_error)
  SUB_BINARY_SENSOR(s4_error)

 protected:
  // Latches so a mismatched model/truncated telegram warns once instead of on every telegram (~1/s).
  bool warned_short_message_{false};

  void dump_sensor_errors_(const LogString *model);
  void publish_sensor_errors_(const std::vector<uint8_t> &message, size_t offset);
  void warn_short_message_(size_t actual, size_t expected);
};

class DeltaSolBSPlusBSensor final : public DeltaSolErrorsBSensorBase {
 public:
  void dump_config() override;
  SUB_BINARY_SENSOR(relay1)
  SUB_BINARY_SENSOR(relay2)
  SUB_BINARY_SENSOR(collector_max)
  SUB_BINARY_SENSOR(collector_min)
  SUB_BINARY_SENSOR(collector_frost)
  SUB_BINARY_SENSOR(tube_collector)
  SUB_BINARY_SENSOR(recooling)
  SUB_BINARY_SENSOR(hqm)

 protected:
  void handle_message(std::vector<uint8_t> &message) override;
};

class DeltaSolBS2009BSensor final : public DeltaSolErrorsBSensorBase {
 public:
  void dump_config() override;
  SUB_BINARY_SENSOR(frost_protection_active)

 protected:
  void handle_message(std::vector<uint8_t> &message) override;
};

class DeltaSolCBSensor final : public DeltaSolErrorsBSensorBase {
 public:
  void dump_config() override { this->dump_sensor_errors_(LOG_STR("Deltasol C")); }

 protected:
  void handle_message(std::vector<uint8_t> &message) override { this->publish_sensor_errors_(message, 10); }
};

class DeltaSolCS2BSensor final : public DeltaSolErrorsBSensorBase {
 public:
  void dump_config() override { this->dump_sensor_errors_(LOG_STR("Deltasol CS2")); }

 protected:
  void handle_message(std::vector<uint8_t> &message) override { this->publish_sensor_errors_(message, 18); }
};

class DeltaSolCS4BSensor final : public DeltaSolErrorsBSensorBase {
 public:
  void dump_config() override { this->dump_sensor_errors_(LOG_STR("Deltasol CS4")); }

 protected:
  void handle_message(std::vector<uint8_t> &message) override { this->publish_sensor_errors_(message, 20); }
};

class DeltaSolCSPlusBSensor final : public DeltaSolErrorsBSensorBase {
 public:
  void dump_config() override { this->dump_sensor_errors_(LOG_STR("Deltasol CS Plus")); }

 protected:
  void handle_message(std::vector<uint8_t> &message) override { this->publish_sensor_errors_(message, 20); }
};

class DeltaSolBS2BSensor final : public DeltaSolErrorsBSensorBase {
 public:
  void dump_config() override { this->dump_sensor_errors_(LOG_STR("DeltaSol BS/2 (DrainBack)")); }

 protected:
  void handle_message(std::vector<uint8_t> &message) override { this->publish_sensor_errors_(message, 10); }
};

class VBusCustomSubBSensor;

class VBusCustomBSensor final : public VBusListener, public Component {
 public:
  void dump_config() override;
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
