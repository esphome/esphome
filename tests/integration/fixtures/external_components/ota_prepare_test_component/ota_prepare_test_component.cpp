#include "ota_prepare_test_component.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::ota_prepare_test_component {

static const char *const TAG = "ota_prepare_test";

void OTAPrepareTestComponent::loop() {
  if (!this->pending_)
    return;
  this->loops_++;
  if (App.get_loop_component_start_time() - this->noticed_at_ < this->ready_after_)
    return;
  ESP_LOGI(TAG, "Prepared after %" PRIu32 " loop passes", this->loops_);
  this->pending_ = false;
  this->prepared_ = true;
}

void OTAPrepareTestComponent::on_ota_prepare() {
  ESP_LOGI(TAG, "Prepare requested, ready after %" PRIu32 " ms", this->ready_after_);
  // Same clock as loop(), so the difference cannot wrap
  this->noticed_at_ = App.get_loop_component_start_time();
  this->loops_ = 0;
  this->prepared_ = this->ready_after_ == 0;
  this->pending_ = !this->prepared_;
}

}  // namespace esphome::ota_prepare_test_component
