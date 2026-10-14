#pragma once

#include "esphome/core/defines.h"
#if defined(USE_ESP32) && defined(USE_NUMBER)

#include "ecocomfort2_hub.h"
#include "esphome/components/number/number.h"

namespace esphome::ecocomfort2 {

class Ecocomfort2Number final : public number::Number, public Parented<Ecocomfort2Hub> {
 public:
  explicit Ecocomfort2Number(NumberSetting setting) : setting_(setting) {}

 protected:
  void control(float value) override {
    if (this->parent_->write_number(this->setting_, value))
      this->publish_state(value);
  }

  const NumberSetting setting_;
};

}  // namespace esphome::ecocomfort2

#endif
