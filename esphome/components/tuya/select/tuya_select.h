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
  void set_select_id(uint8_t select_id) { this->select_id_ = select_id; }

  void set_tuya_parent(Tuya *parent) { this->parent_ = parent; }
  void set_enums(std::vector<std::uint8_t> enums) { this->enums_ = enums; }

 protected:
  void control(const std::string &value) override;
  const std::string& enum_to_value(uint8_t enum_value);
  std::uint8_t value_to_enum(const std::string& value);

  Tuya *parent_;
  uint8_t select_id_{0};
  std::vector<std::uint8_t> enums_;
  std::string error_not_found {"Error: not found value"};
};

}  // namespace tuya
}  // namespace esphome
