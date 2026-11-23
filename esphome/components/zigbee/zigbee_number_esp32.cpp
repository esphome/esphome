#include "zigbee_number_esp32.h"
#include "esphome/core/log.h"
#include "esphome/core/defines.h"
#if defined(USE_ESP32) && defined(USE_ZIGBEE) && defined(USE_NUMBER)

namespace esphome::zigbee {

ESPHOME_LOG_TAG(TAG, "zigbee.number");

namespace {

class FlagGuard {
 public:
  explicit FlagGuard(bool &flag) : flag_(flag) { this->flag_ = true; }
  ~FlagGuard() { this->flag_ = false; }
  FlagGuard(const FlagGuard &) = delete;
  FlagGuard &operator=(const FlagGuard &) = delete;

 private:
  bool &flag_;
};

}  // namespace

void ZigbeeNumber::setup() {
  const bool has_state = this->number_->has_state();
  this->add_attr(0, has_state ? this->number_->state : NAN, &this->value_);
  if (this->force_report_ && has_state) {
    this->report();
  }

  // setup static attributes
  this->zb_->add_attr(this->endpoint_id_, this->cluster_id_, this->role_,
                      EZB_ZCL_ATTR_ANALOG_OUTPUT_MIN_PRESENT_VALUE_ID, 0, EZB_ZCL_ATTR_TYPE_SINGLE, 0,
                      this->number_->traits.get_min_value());
  this->zb_->add_attr(this->endpoint_id_, this->cluster_id_, this->role_,
                      EZB_ZCL_ATTR_ANALOG_OUTPUT_MAX_PRESENT_VALUE_ID, 0, EZB_ZCL_ATTR_TYPE_SINGLE, 0,
                      this->number_->traits.get_max_value());
  this->zb_->add_attr(this->endpoint_id_, this->cluster_id_, this->role_, EZB_ZCL_ATTR_ANALOG_OUTPUT_RESOLUTION_ID, 0,
                      EZB_ZCL_ATTR_TYPE_SINGLE, 0, this->number_->traits.get_step());

  // connect callbacks
  this->number_->add_on_state_callback([this](float value) {
    if (this->synchronizing_from_zigbee_ && value == this->value_) {
      // The attribute already holds it; only a forced report is still owed
      if (this->force_report_)
        this->report();
      return;
    }
    this->set_attr(value);
  });
}

void ZigbeeNumber::on_value(ezb_zcl_attribute_t attribute) {
  if (attribute.data.type == this->attr_type() && attribute.data.value) {
    const float incoming_value = *static_cast<float *>(attribute.data.value);
    this->value_ = incoming_value;
    this->set_attr_requested_ = false;
    auto call = this->number_->make_call();
    call.set_value(incoming_value);
    FlagGuard guard(this->synchronizing_from_zigbee_);
    call.perform();
  }
}

}  // namespace esphome::zigbee

#endif
