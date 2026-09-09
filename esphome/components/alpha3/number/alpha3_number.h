#pragma once

#include "esphome/components/alpha3/alpha3.h"

#ifdef USE_NUMBER
namespace esphome::alpha3 {

class Alpha3Number : public number::Number {
 public:
  Alpha3Number(Alpha3 *parent, Alpha3NumberType type) : parent_(parent), type_(type) {}

 protected:
  void control(float value) override;
  Alpha3 *parent_;
  Alpha3NumberType type_;
};

}  // namespace esphome::alpha3
#endif
