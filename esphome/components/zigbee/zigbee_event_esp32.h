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
  // IMPORTANT: We MUST copy all values because the pointer from ESP-IDF
  // is only valid during the callback execution. Since ZB events are processed
  // asynchronously in the main loop, we store our own copy inline to ensure
  // the data remains valid until the event is processed.
  ZBEvent(ezb_zcl_message_info_t info, ezb_zcl_attribute_t attribute) {
    this->callback_id_ = EZB_ZCL_CORE_SET_ATTR_VALUE_CB_ID;
    this->init_set_attr_value_data_(info, attribute);
  }

  // Destructor to clean up heap allocations
  ~ZBEvent() { this->release(); }

  // Default constructor for pre-allocation in pool
  ZBEvent() : event_{}, callback_id_(EZB_ZCL_CORE_CB_ID_END) {}

  // Invoked on return to EventPool - clean up any heap-allocated data
  void release() {
    // Free any allocated memory within the event
    switch (this->callback_id_) {
      case EZB_ZCL_CORE_SET_ATTR_VALUE_CB_ID:
        if (!this->event_.set_attr.is_inline && this->event_.set_attr.data.heap_data != nullptr) {
          delete[] this->event_.set_attr.data.heap_data;
          this->event_.set_attr.data.heap_data = nullptr;
        }
        break;
      default:
        break;
    }
  }

  // Load new event data for reuse (replaces previous event data)
  // Note: release() is NOT called here because EventPool::release() already
  // calls event->release() before returning to the free list. Every event
  // from allocate() is already in a clean state.
  void load_set_attr_value_event(ezb_zcl_message_info_t info, ezb_zcl_attribute_t attribute) {
    this->callback_id_ = EZB_ZCL_CORE_SET_ATTR_VALUE_CB_ID;
    this->init_set_attr_value_data_(info, attribute);
  }

  // Disable copy to prevent double-delete
  ZBEvent(const ZBEvent &) = delete;
  ZBEvent &operator=(const ZBEvent &) = delete;

  union {
    // NOLINTNEXTLINE(readability-identifier-naming)
    struct set_attr_event {
      ezb_zcl_message_info_t info;
      ezb_zcl_attribute_t attribute;
      union {
        uint8_t *heap_data;
        uint8_t inline_data[4];  // For small data types (<= 32 bit)
      } data;
      bool is_inline;
    } set_attr;
  } event_;

  ezb_zcl_core_action_callback_id_t callback_id_;

 private:
  void init_set_attr_value_data_(ezb_zcl_message_info_t info, ezb_zcl_attribute_t attribute) {
    this->event_.set_attr.info = info;
    this->event_.set_attr.attribute = attribute;
    // get attribute.data.value with correct type
    if (attribute.data.value != nullptr) {
      // Copy the attribute value to avoid dangling pointer issues
      size_t value_size = ezb_zcl_get_attr_value_size(attribute.data.type, attribute.data.value);
      if (value_size > 4) {
        this->event_.set_attr.data.heap_data = new uint8_t[value_size];
        memcpy(this->event_.set_attr.data.heap_data, attribute.data.value, value_size);
        this->event_.set_attr.attribute.data.value = this->event_.set_attr.data.heap_data;
        this->event_.set_attr.is_inline = false;
      } else {
        memcpy(this->event_.set_attr.data.inline_data, attribute.data.value, value_size);
        this->event_.set_attr.attribute.data.value = this->event_.set_attr.data.inline_data;
        this->event_.set_attr.is_inline = true;
      }
    }
  }
};
}  // namespace esphome::zigbee
#endif  // USE_ZIGBEE
#endif  // USE_ESP32
