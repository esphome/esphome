#pragma once

#include "zigbee_device_base_esp32.h"

#ifdef USE_ESP32
#ifdef USE_ZIGBEE
#ifdef USE_LIGHT

#include "esphome/components/light/light_state.h"
#include "esp_zigbee.h"
#include "zigbee_esp32.h"

namespace esphome::zigbee {

template<typename T, typename Owner> class ZigbeeOnValueForwarder final : public ZigbeeAttributeInterface {
 public:
  using Callback = void (Owner::*)(T);

  ZigbeeOnValueForwarder(Owner *owner, uint8_t type, Callback callback, bool report)
      : owner_(owner), type_(type), callback_(callback), report_(report) {}

  void on_value(ezb_zcl_attribute_t attribute) override {
    if (attribute.data.value && attribute.data.type == this->type_) {
      (this->owner_->*this->callback_)(*static_cast<T *>(attribute.data.value));
    }
  }

  void report() override {
    if (this->report_)
      this->owner_->report();
  }

 protected:
  Owner *owner_;
  uint8_t type_;
  Callback callback_;
  bool report_;
};

class ZigbeeLight final : public ZigbeeDeviceBase, public light::LightRemoteValuesListener {
 public:
  ZigbeeLight(ZigbeeComponent *parent, uint8_t endpoint_id)
      : ZigbeeDeviceBase(parent, endpoint_id),
        on_off_forwarder_(this, EZB_ZCL_ATTR_TYPE_BOOL, &ZigbeeLight::apply_on_off_, true) {}

  void setup() override;
  void on_light_remote_values_update() override;
  void on_value(ezb_zcl_attribute_t attribute) override {};
  void connect(light::LightState *light_state);
  // Setup before ZigbeeComponent
  float get_setup_priority() const override { return setup_priority::WIFI + 1; }

 protected:
  void schedule_call_();
  void make_call_();
  bool loop_internal() override;
  void set_attr_internal() override;
  void report_internal(bool has_lock) override;
  void apply_on_off_(bool on);
  void apply_level_(uint8_t level);
  ZigbeeOnValueForwarder<bool, ZigbeeLight> on_off_forwarder_;
  ZigbeeOnValueForwarder<uint8_t, ZigbeeLight> *level_forwarder_{nullptr};
  light::LightState *light_state_;
  light::LightCall next_call_ = light::LightCall(nullptr);
  bool synchronizing_from_zigbee_{false};
  bool call_pending_{false};
  bool block_zigbee_{false};
};

}  // namespace esphome::zigbee

#endif
#endif
#endif
