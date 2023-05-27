#pragma once

#include "../vbus.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace vbus {

class DeltaSolBSPlusBSensor : public VBusListener, public Component {
 public:
  void dump_config() override;
  void set_relay1_bsensor(binary_sensor::BinarySensor *bsensor) { this->relay1_bsensor_ = bsensor; }
  void set_relay2_bsensor(binary_sensor::BinarySensor *bsensor) { this->relay2_bsensor_ = bsensor; }
  void set_s1_error_bsensor(binary_sensor::BinarySensor *bsensor) { this->s1_error_bsensor_ = bsensor; }
  void set_s2_error_bsensor(binary_sensor::BinarySensor *bsensor) { this->s2_error_bsensor_ = bsensor; }
  void set_s3_error_bsensor(binary_sensor::BinarySensor *bsensor) { this->s3_error_bsensor_ = bsensor; }
  void set_s4_error_bsensor(binary_sensor::BinarySensor *bsensor) { this->s4_error_bsensor_ = bsensor; }
  void set_collector_max_bsensor(binary_sensor::BinarySensor *bsensor) { this->collector_max_bsensor_ = bsensor; }
  void set_collector_min_bsensor(binary_sensor::BinarySensor *bsensor) { this->collector_min_bsensor_ = bsensor; }
  void set_collector_frost_bsensor(binary_sensor::BinarySensor *bsensor) { this->collector_frost_bsensor_ = bsensor; }
  void set_tube_collector_bsensor(binary_sensor::BinarySensor *bsensor) { this->tube_collector_bsensor_ = bsensor; }
  void set_recooling_bsensor(binary_sensor::BinarySensor *bsensor) { this->recooling_bsensor_ = bsensor; }
  void set_hqm_bsensor(binary_sensor::BinarySensor *bsensor) { this->hqm_bsensor_ = bsensor; }

 protected:
  binary_sensor::BinarySensor *relay1_bsensor_{nullptr};
  binary_sensor::BinarySensor *relay2_bsensor_{nullptr};
  binary_sensor::BinarySensor *s1_error_bsensor_{nullptr};
  binary_sensor::BinarySensor *s2_error_bsensor_{nullptr};
  binary_sensor::BinarySensor *s3_error_bsensor_{nullptr};
  binary_sensor::BinarySensor *s4_error_bsensor_{nullptr};
  binary_sensor::BinarySensor *collector_max_bsensor_{nullptr};
  binary_sensor::BinarySensor *collector_min_bsensor_{nullptr};
  binary_sensor::BinarySensor *collector_frost_bsensor_{nullptr};
  binary_sensor::BinarySensor *tube_collector_bsensor_{nullptr};
  binary_sensor::BinarySensor *recooling_bsensor_{nullptr};
  binary_sensor::BinarySensor *hqm_bsensor_{nullptr};

  void handle_message(std::vector<uint8_t> &message) override;
};

class DeltaSolCBSensor : public VBusListener, public Component {
 public:
  void dump_config() override;
  void set_s1_error_bsensor(binary_sensor::BinarySensor *bsensor) { this->s1_error_bsensor_ = bsensor; }
  void set_s2_error_bsensor(binary_sensor::BinarySensor *bsensor) { this->s2_error_bsensor_ = bsensor; }
  void set_s3_error_bsensor(binary_sensor::BinarySensor *bsensor) { this->s3_error_bsensor_ = bsensor; }
  void set_s4_error_bsensor(binary_sensor::BinarySensor *bsensor) { this->s4_error_bsensor_ = bsensor; }

 protected:
  binary_sensor::BinarySensor *s1_error_bsensor_{nullptr};
  binary_sensor::BinarySensor *s2_error_bsensor_{nullptr};
  binary_sensor::BinarySensor *s3_error_bsensor_{nullptr};
  binary_sensor::BinarySensor *s4_error_bsensor_{nullptr};

  void handle_message(std::vector<uint8_t> &message) override;
};

class DeltaSolCS2BSensor : public VBusListener, public Component {
 public:
  void dump_config() override;
  void set_s1_error_bsensor(binary_sensor::BinarySensor *bsensor) { this->s1_error_bsensor_ = bsensor; }
  void set_s2_error_bsensor(binary_sensor::BinarySensor *bsensor) { this->s2_error_bsensor_ = bsensor; }
  void set_s3_error_bsensor(binary_sensor::BinarySensor *bsensor) { this->s3_error_bsensor_ = bsensor; }
  void set_s4_error_bsensor(binary_sensor::BinarySensor *bsensor) { this->s4_error_bsensor_ = bsensor; }

 protected:
  binary_sensor::BinarySensor *s1_error_bsensor_{nullptr};
  binary_sensor::BinarySensor *s2_error_bsensor_{nullptr};
  binary_sensor::BinarySensor *s3_error_bsensor_{nullptr};
  binary_sensor::BinarySensor *s4_error_bsensor_{nullptr};

  void handle_message(std::vector<uint8_t> &message) override;
};

class DeltaSolCSPlusBSensor : public VBusListener, public Component {
 public:
  void dump_config() override;
  void set_s1_error_bsensor(binary_sensor::BinarySensor *bsensor) { this->s1_error_bsensor_ = bsensor; }
  void set_s2_error_bsensor(binary_sensor::BinarySensor *bsensor) { this->s2_error_bsensor_ = bsensor; }
  void set_s3_error_bsensor(binary_sensor::BinarySensor *bsensor) { this->s3_error_bsensor_ = bsensor; }
  void set_s4_error_bsensor(binary_sensor::BinarySensor *bsensor) { this->s4_error_bsensor_ = bsensor; }

 protected:
  binary_sensor::BinarySensor *s1_error_bsensor_{nullptr};
  binary_sensor::BinarySensor *s2_error_bsensor_{nullptr};
  binary_sensor::BinarySensor *s3_error_bsensor_{nullptr};
  binary_sensor::BinarySensor *s4_error_bsensor_{nullptr};

  void handle_message(std::vector<uint8_t> &message) override;
};

class VBusCustomSubBSensor;

class VBusCustomBSensor : public VBusListener, public Component {
 public:
  void dump_config() override;
  void set_bsensors(std::vector<VBusCustomSubBSensor *> bsensors) { this->bsensors_ = std::move(bsensors); };

 protected:
  std::vector<VBusCustomSubBSensor *> bsensors_;
  void handle_message(std::vector<uint8_t> &message) override;
};

class VBusCustomSubBSensor : public binary_sensor::BinarySensor, public Component {
 public:
  void set_message_parser(message_parser_t parser) { this->message_parser_ = std::move(parser); };
  void parse_message(std::vector<uint8_t> &message);

 protected:
  message_parser_t message_parser_;
};

}  // namespace vbus
}  // namespace esphome
