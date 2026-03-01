#pragma once
#include "../bthome_handler.h"
#include "../bthome_local_sensor.h"
#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace bthome {

class BTHomeBinarySensor : public BTHomeObjectHandler, public esphome::binary_sensor::BinarySensor, public Component {
 public:
  bool process_object(const BTHomeObject &object) override;
};

namespace server {
class BTHomeLocalBinarySensor : public BTHomeLocalBase {
 public:
  void set_source(binary_sensor::BinarySensor *source) { this->source_ = source; }
  size_t get_encoded_size() const override {
    if (!source_->has_state()) {
      return 0;  // Don't include in frame if state is not valid
    }
    return sizeof(BTHomeHeader) + get_bthome_value_length(this->object_type_);
  }
  bool write(BTHomeEncoder &encoder) const override {
    return encoder.write_bool(this->object_type_, this->source_->state);
  }
  void register_immediate_callback(std::function<void()> &&callback) override {
    this->source_->add_on_state_callback([callback](bool) { callback(); });
  }

 protected:
  binary_sensor::BinarySensor *source_{nullptr};
};
}  // namespace server

}  // namespace bthome
}  // namespace esphome
