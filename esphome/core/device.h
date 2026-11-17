#pragma once

#include <cstdint>

#include "esphome/core/defines.h"

namespace esphome {

/// How a sub-device relates to the node that exposes it. Values match the API's DeviceRelation enum.
enum class DeviceRelation : uint8_t {
  /// A separate physical device reached through this node (for example over RF, UART or ESP-NOW).
  DEVICE_RELATION_VIA = 0,
  /// A logical part of this node (for example one zone of a multi-zone controller).
  DEVICE_RELATION_CHILD = 1,
};

class Device {
 public:
  void set_device_id(uint32_t device_id) { this->device_id_ = device_id; }
  uint32_t get_device_id() { return this->device_id_; }
  void set_name(const char *name) { this->name_ = name; }
  const char *get_name() { return this->name_; }
  void set_area_id(uint32_t area_id) { this->area_id_ = area_id; }
  uint32_t get_area_id() { return this->area_id_; }
#if defined(USE_DEVICE_RELATION)
  // Only emitted when devices have different relations; otherwise the getter is a constant and costs no RAM.
  void set_relation(DeviceRelation relation) { this->relation_ = relation; }
  DeviceRelation get_relation() const { return this->relation_; }
#elif defined(USE_DEVICE_RELATION_ALL_CHILD)
  DeviceRelation get_relation() const { return DeviceRelation::DEVICE_RELATION_CHILD; }
#else
  DeviceRelation get_relation() const { return DeviceRelation::DEVICE_RELATION_VIA; }
#endif

 protected:
  uint32_t device_id_{};
  uint32_t area_id_{};
  const char *name_ = "";
#ifdef USE_DEVICE_RELATION
  DeviceRelation relation_{DeviceRelation::DEVICE_RELATION_VIA};
#endif
};

}  // namespace esphome
