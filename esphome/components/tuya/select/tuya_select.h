#pragma once

#include "esphome/core/component.h"
#include "esphome/components/tuya/tuya.h"
#include "esphome/components/select/select.h"

namespace esphome {
namespace tuya {

class TuyaSelect : public select::Select, public Component {
 public:
  void setup() override;
  void dump_config() override;

  void set_tuya_parent(Tuya *parent) { this->parent_ = parent; }
  void set_optimistic(bool optimistic) { this->optimistic_ = optimistic; }
  void set_select_id(uint8_t select_id) { this->select_id_ = select_id; }
 protected:
  void control(const std::string &value) override;

  Tuya *parent_;
  bool optimistic_ = false;
  uint8_t select_id_{0};
};

}  // namespace tuya
}  // namespace esphome
