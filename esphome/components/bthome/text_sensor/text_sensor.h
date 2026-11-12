#pragma once
#include "../remote_object.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/helpers.h"

#include <cstdint>
#include <span>

namespace esphome::bthome::client {

class BTHomeTextSensor : public BTHomeRemoteObject, public esphome::text_sensor::TextSensor {
 public:
  bool process_object(const BTHomeObject &object) override;
  void log_config() override;

  /// Called with the original bytes of each RAW object, before they are published as hex.
  template<typename F> void add_on_raw_data_callback(F &&callback) {
    this->raw_data_callback_.add(std::forward<F>(callback));
  }

 protected:
  LazyCallbackManager<void(std::span<const uint8_t>)> raw_data_callback_;
};

}  // namespace esphome::bthome::client
