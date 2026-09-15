#pragma once

#include "esphome/components/alpha3/alpha3.h"

#if defined(USE_ESP32) && defined(USE_SELECT)
namespace esphome::alpha3 {

class Alpha3Select final : public select::Select {
 public:
  Alpha3Select(Alpha3 *parent, Alpha3SelectType type) : parent_(parent), type_(type) {}

 protected:
  void control(size_t index) override;
  Alpha3 *parent_;
  Alpha3SelectType type_;
};

}  // namespace esphome::alpha3
#endif
