#pragma once

namespace esphome {

class Device {
 public:
  void set_device_id(uint32_t device_id) { this->device_id_ = device_id; }
  uint32_t get_device_id() { return this->device_id_; }
  void set_name(const char *name) { this->name_ = name; }
  const char *get_name() { return this->name_; }
  void set_area_id(uint32_t area_id) { this->area_id_ = area_id; }
  uint32_t get_area_id() { return this->area_id_; }

 protected:
  uint32_t device_id_{};
  uint32_t area_id_{};
  const char *name_ = "";
};

}  // namespace esphome
