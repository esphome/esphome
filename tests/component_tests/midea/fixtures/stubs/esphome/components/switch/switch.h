#pragma once
namespace esphome::switch_ {
// Only the framework boundary is replaced; the Midea bridge is compiled unchanged.
class Switch {
 public:
  virtual ~Switch() = default;
  bool state{false};
  int publications{0};
  bool has_state() const { return this->publications != 0; }
  void publish_state(bool value) {
    this->state = value;
    ++this->publications;
  }
  void turn_on() { this->write_state(true); }
  void turn_off() { this->write_state(false); }

 protected:
  virtual void write_state(bool state) = 0;
};
}  // namespace esphome::switch_
