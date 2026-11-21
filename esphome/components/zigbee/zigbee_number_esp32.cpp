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
  if (this->device_->has_state()) {
    value = this->device_->state;
  }
  this->add_attr(0, value);

  // setup static attributes
  this->zb_->add_attr(this->endpoint_id_, this->cluster_id_, this->role_,
                      EZB_ZCL_ATTR_ANALOG_OUTPUT_MIN_PRESENT_VALUE_ID, 0, EZB_ZCL_ATTR_TYPE_SINGLE, 0,
                      this->device_->traits.get_min_value());
  this->zb_->add_attr(this->endpoint_id_, this->cluster_id_, this->role_,
                      EZB_ZCL_ATTR_ANALOG_OUTPUT_MAX_PRESENT_VALUE_ID, 0, EZB_ZCL_ATTR_TYPE_SINGLE, 0,
                      this->device_->traits.get_max_value());
  this->zb_->add_attr(this->endpoint_id_, this->cluster_id_, this->role_, EZB_ZCL_ATTR_ANALOG_OUTPUT_RESOLUTION_ID, 0,
                      EZB_ZCL_ATTR_TYPE_SINGLE, 0, this->device_->traits.get_step());

  // connect callbacks
  this->add_on_value_callback([this](ezb_zcl_attribute_t attribute) {
    auto call = this->device_->make_call();
    call.set_value(*(float *) attribute.data.value);
    this->synchronizing_from_zigbee_ = true;
    call.perform();
    this->synchronizing_from_zigbee_ = false;
  });
  this->device_->add_on_state_callback([this](float value) {
    if (this->synchronizing_from_zigbee_)
      return;
    this->set_attr(this->scale_value_<float>(this->device_->state));
  });
}

}  // namespace esphome::zigbee

#endif
#endif
#endif
