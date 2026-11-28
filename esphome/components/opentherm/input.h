#pragma once

namespace esphome::opentherm {

class OpenthermInput {
 public:
  bool auto_min_value, auto_max_value;

  virtual void set_auto_min_value(bool auto_min_value) { this->auto_min_value = auto_min_value; }
  virtual void set_auto_max_value(bool auto_max_value) { this->auto_max_value = auto_max_value; }
};

}  // namespace esphome::opentherm
