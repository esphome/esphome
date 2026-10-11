#pragma once
#include <cstdint>
namespace esphome {
namespace setup_priority {
constexpr float BEFORE_CONNECTION = 200;
}
class Component {
 public:
  virtual ~Component() = default;
  virtual void setup() {}
  virtual void loop() {}
  virtual void dump_config() {}
  virtual float get_setup_priority() const { return 600; }
  virtual bool can_proceed() { return true; }
};
}  // namespace esphome
