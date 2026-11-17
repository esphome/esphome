#pragma once

#include "esphome/core/helpers.h"
#include "esphome/components/tuya/tuya.h"
#include "esphome/components/select/select.h"

namespace esphome::tuya {

class TuyaSelect final : public select::Select {
 public:
  // Called from generated code before setup, after the datapoint setters
  void register_listeners();
  // Called from the hub's dump_config()
  void dump_child_config();

  void set_tuya_parent(Tuya *parent) { this->parent_ = parent; }
  void set_optimistic(bool optimistic) { this->optimistic_ = optimistic; }
  void set_select_id(uint8_t select_id, TuyaDatapointType datapoint_type) {
    this->select_id_ = select_id;
    this->datapoint_type_ = datapoint_type;
  }
  void set_select_mappings(std::initializer_list<uint8_t> mappings) { this->mappings_ = mappings; }

 protected:
  void control(size_t index) override;

  Tuya *parent_;
  bool optimistic_ = false;
  uint8_t select_id_;
  FixedVector<uint8_t> mappings_;
  TuyaDatapointType datapoint_type_ = TuyaDatapointType::ENUM;
};

}  // namespace esphome::tuya
