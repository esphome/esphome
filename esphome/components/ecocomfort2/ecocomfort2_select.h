#pragma once

#include "esphome/core/defines.h"
#if defined(USE_ESP32) && defined(USE_SELECT)

#include "ecocomfort2_hub.h"
#include "esphome/components/select/select.h"

namespace esphome::ecocomfort2 {

class Ecocomfort2Select final : public select::Select, public Parented<Ecocomfort2Hub> {
 public:
  explicit Ecocomfort2Select(SelectSetting setting) : setting_(setting) {}

 protected:
  void control(size_t index) override { this->parent_->write_select(this->setting_, index); }

  const SelectSetting setting_;
};

}  // namespace esphome::ecocomfort2

#endif
