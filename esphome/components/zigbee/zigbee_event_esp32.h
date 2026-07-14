#pragma once

#include "esphome/core/defines.h"
#ifdef USE_ESP32
#ifdef USE_ZIGBEE

#include <cstddef>  // for offsetof
#include <cstring>  // for memcpy
#include "esp_zigbee.h"
#include "ezbee/zha.h"

namespace esphome::zigbee {

class ZBEvent {
 public:
  ZBEvent(ezb_zcl_message_info_t info, ezb_zcl_attribute_t attribute) {
    this->callback_id_ = EZB_ZCL_CORE_SET_ATTR_VALUE_CB_ID;
    this->init_set_attr_value_data(info, attribute);
  }

  ~ZBEvent() { this->release(); }

  ZBEvent() : event_{}, callback_id_(EZB_ZCL_CORE_CB_ID_END) {}

  void release() {
    // Free any allocated memory within the event
    switch (this->callback_id_) {
      case EZB_ZCL_CORE_SET_ATTR_VALUE_CB_ID:
        if (this->event_.set_attr.attribute.data.value != nullptr &&
            this->event_.set_attr.attribute.data.value != this->event_.set_attr.inline_data) {
          free(this->event_.set_attr.attribute.data.value);
          this->event_.set_attr.attribute.data.value = nullptr;
        }
        break;
      default:
        break;
    }
  }

  void load_set_attr_value_event(ezb_zcl_message_info_t info, ezb_zcl_attribute_t attribute) {
    this->release();
    this->callback_id_ = EZB_ZCL_CORE_SET_ATTR_VALUE_CB_ID;
    this->init_set_attr_value_data(info, attribute);
  }

  // Disable copy to prevent double-delete
  ZBEvent(const ZBEvent &) = delete;
  ZBEvent &operator=(const ZBEvent &) = delete;

  union {
    struct set_attr_event {
      ezb_zcl_message_info_t info;
      ezb_zcl_attribute_t attribute;
      uint8_t inline_data[4];  // For small data types (<= 32 bit)
    } set_attr;
  } event_;

  ezb_zcl_core_action_callback_id_t callback_id_;

 private:
  void init_set_attr_value_data(ezb_zcl_message_info_t info, ezb_zcl_attribute_t attribute) {
    this->event_.set_attr.info = info;
    this->event_.set_attr.attribute = attribute;
    // get attribute.data.value with correct type
    if (attribute.data.value != nullptr) {
      // Copy the attribute value to avoid dangling pointer issues
      size_t value_size = ezb_zcl_get_attr_value_size(attribute.data.type, attribute.data.value);
      if (value_size > 4) {
        this->event_.set_attr.attribute.data.value = malloc(value_size);
        if (this->event_.set_attr.attribute.data.value != nullptr) {
          memcpy(this->event_.set_attr.attribute.data.value, attribute.data.value, value_size);
        }
      } else {
        memcpy(this->event_.set_attr.inline_data, attribute.data.value, value_size);
        this->event_.set_attr.attribute.data.value = this->event_.set_attr.inline_data;
      }
    }
  }
};
}  // namespace esphome::zigbee
#endif  // USE_ZIGBEE
#endif  // USE_ESP32
