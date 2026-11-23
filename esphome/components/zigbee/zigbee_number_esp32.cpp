#include "zigbee_number_esp32.h"
#include "esphome/core/log.h"
#include "esphome/core/defines.h"
#ifdef USE_ESP32
#ifdef USE_ZIGBEE
#ifdef USE_NUMBER

namespace esphome::zigbee {

ESPHOME_LOG_TAG(TAG, "zigbee.number");

void ZigbeeNumber::setup() {
  float value = std::numeric_limits<float>::quiet_NaN();
  if (this->number_->has_state()) {
    value = this->number_->state;
  }
  this->add_attr(0, value);
  if (this->force_report_ && this->number_->has_state()) {
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
    if (this->synchronizing_from_zigbee_ && value == *static_cast<float *>(this->value_p_))
      return;
    this->set_attr(value);
  });
}

void ZigbeeNumber::on_value(ezb_zcl_attribute_t attribute) {
  if (attribute.data.type == this->attr_type() && attribute.data.value) {
    const float incoming_value = *static_cast<float *>(attribute.data.value);
    *static_cast<float *>(this->value_p_) = incoming_value;
    this->set_attr_requested_ = false;
    auto call = this->number_->make_call();
    call.set_value(incoming_value);
    this->synchronizing_from_zigbee_ = true;
    call.perform();
    this->synchronizing_from_zigbee_ = false;
  }
}

}  // namespace esphome::zigbee

#endif
#endif
#endif
