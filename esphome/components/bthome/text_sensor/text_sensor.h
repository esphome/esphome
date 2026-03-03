#pragma once
#include "../local_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include <algorithm>

namespace esphome {
namespace bthome {
namespace server {

class BTHomeLocalTextSensor : public BTHomeLocalBase {
 public:
  void set_source(text_sensor::TextSensor *source) { this->source_ = source; }
  void set_max_length(size_t max_length) { this->max_length_ = max_length; }

  size_t get_encoded_size() const override {
    if (!this->source_->has_state())
      return 0;
    size_t content_len = std::min(this->source_->state.size(), this->max_length_);
    return 1 + 1 + content_len;  // type + length_byte + content
  }

  bool write(BTHomeEncoder &encoder) const override {
    return encoder.write_text(this->object_type_, this->source_->state.c_str(), this->source_->state.size(),
                              this->max_length_);
  }

  void register_immediate_callback(std::function<void()> &&callback) override {
    this->source_->add_on_state_callback([callback](const std::string &) { callback(); });
  }

 protected:
  text_sensor::TextSensor *source_{nullptr};
  size_t max_length_{5};
};

}  // namespace server
}  // namespace bthome
}  // namespace esphome
