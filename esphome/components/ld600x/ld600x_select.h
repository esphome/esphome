#pragma once

#include "esphome/core/defines.h"

#ifdef USE_SELECT

#include "esphome/components/select/select.h"
#include "ld600x.h"

namespace esphome::ld600x {

class LD600XSelect final : public select::Select, public Parented<LD600XComponent> {
 public:
  explicit LD600XSelect(uint8_t kind) : kind_(kind) {}

 protected:
  void control(size_t index) override;

  uint8_t kind_;
};

}  // namespace esphome::ld600x

#endif  // USE_SELECT
