#pragma once

#include "esphome/core/hal.h"
#include "esphome/core/component.h"
#include "esphome/components/adafruit_seesaw/adafruit_seesaw.h"

namespace esphome {
namespace af4991 {

class AF4991 : public Component, public adafruit_seesaw::AdafruitSeesaw {
 public:
  void setup() override;
  void dump_config() override;

 protected:
  uint32_t product_version_ = 0;
  enum ErrorCode {
    NONE = 0,
    INCORRECT_FIRMWARE_DETECTED,
  } error_code_{NONE};
};

}  // namespace af4991
}  // namespace esphome
