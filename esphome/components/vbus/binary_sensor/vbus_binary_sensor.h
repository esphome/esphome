#pragma once

#include "../vbus.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace vbus {

using message_handler_t = std::function<void(std::vector<uint8_t> &)>;

class DeltaSol_C_bsensor : public VBusListener, public Component {
 public:
  void dump_config() override;
  void set_s1_error_bsensor(binary_sensor::BinarySensor *bsensor) { this->s1_error_bsensor_ = bsensor; }
  void set_s2_error_bsensor(binary_sensor::BinarySensor *bsensor) { this->s2_error_bsensor_ = bsensor; }
  void set_s3_error_bsensor(binary_sensor::BinarySensor *bsensor) { this->s3_error_bsensor_ = bsensor; }
  void set_s4_error_bsensor(binary_sensor::BinarySensor *bsensor) { this->s4_error_bsensor_ = bsensor; }

 protected:
  static constexpr const char *TAG_ = "vbus.deltasol_c.binary";
  binary_sensor::BinarySensor *s1_error_bsensor_{nullptr};
  binary_sensor::BinarySensor *s2_error_bsensor_{nullptr};
  binary_sensor::BinarySensor *s3_error_bsensor_{nullptr};
  binary_sensor::BinarySensor *s4_error_bsensor_{nullptr};

  void handle_message_(std::vector<uint8_t> &message) override;
};

class DeltaSol_CS2_bsensor : public VBusListener, public Component {
 public:
  void dump_config() override;
  void set_s1_error_bsensor(binary_sensor::BinarySensor *bsensor) { this->s1_error_bsensor_ = bsensor; }
  void set_s2_error_bsensor(binary_sensor::BinarySensor *bsensor) { this->s2_error_bsensor_ = bsensor; }
  void set_s3_error_bsensor(binary_sensor::BinarySensor *bsensor) { this->s3_error_bsensor_ = bsensor; }
  void set_s4_error_bsensor(binary_sensor::BinarySensor *bsensor) { this->s4_error_bsensor_ = bsensor; }

 protected:
  static constexpr const char *TAG_ = "vbus.deltasol_cs2.binary";
  binary_sensor::BinarySensor *s1_error_bsensor_{nullptr};
  binary_sensor::BinarySensor *s2_error_bsensor_{nullptr};
  binary_sensor::BinarySensor *s3_error_bsensor_{nullptr};
  binary_sensor::BinarySensor *s4_error_bsensor_{nullptr};

  void handle_message_(std::vector<uint8_t> &message) override;
};

class DeltaSol_BS_Plus_bsensor : public VBusListener, public Component {
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
  static constexpr const char *TAG_ = "vbus.deltasol_bs_plus.binary";
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

  void handle_message_(std::vector<uint8_t> &message) override;
};

}  // namespace vbus
}  // namespace esphome
