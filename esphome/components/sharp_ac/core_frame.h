#pragma once

#include <cstdint>
#include <cstddef>
#include "core_types.h"

namespace esphome {
namespace sharp_ac {

class SharpState;

class SharpFrame {
 protected:
  uint8_t *data_;
  size_t size_;

 public:
  SharpFrame();
  SharpFrame(char c);
  SharpFrame(const uint8_t *arr, size_t sz);
  ~SharpFrame();
  SharpFrame(const SharpFrame &other);
  uint8_t *get_data() const;
  size_t get_size() const;
  int set_size(size_t sz);
  void print();
  virtual void set_checksum();
  bool validate_checksum();
  uint8_t calc_checksum();
};

class SharpStatusFrame : public SharpFrame {
 public:
  SharpStatusFrame(const uint8_t *arr);
  int get_temperature();
};

class SharpModeFrame : public SharpFrame {
 public:
  SharpModeFrame(const uint8_t *arr);
  int get_temperature();
  bool get_state();
  FanMode get_fan_mode();
  PowerMode get_power_mode();
  SwingVertical get_swing_vertical();
  SwingHorizontal get_swing_horizontal();
  Preset get_preset();
  bool get_ion();
};

class SharpCommandFrame : public SharpFrame {
 public:
  SharpCommandFrame();
  void set_data(SharpState *state);
  void set_checksum() override;

 private:
  void command_checksum_();
};

class SharpACKFrame : public SharpFrame {
 public:
  SharpACKFrame();
  void set_checksum() override;
};

}  // namespace sharp_ac
}  // namespace esphome
