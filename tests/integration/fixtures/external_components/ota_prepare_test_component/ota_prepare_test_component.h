#pragma once

#include "esphome/components/ota/ota_backend.h"
#include "esphome/core/component.h"

namespace esphome::ota_prepare_test_component {

// Prepared ready_after ms after the notice, from loop(); 0 means at once.
class OTAPrepareTestComponent : public Component, public ota::OTAPrepareListener {
 public:
  void set_ready_after(uint32_t ready_after) { this->ready_after_ = ready_after; }

  void loop() override;
  void on_ota_prepare() override;
  bool is_ota_prepared() override { return this->prepared_; }

 protected:
  uint32_t ready_after_{0};
  uint32_t noticed_at_{0};
  uint32_t loops_{0};
  bool pending_{false};
  bool prepared_{false};
};

}  // namespace esphome::ota_prepare_test_component
