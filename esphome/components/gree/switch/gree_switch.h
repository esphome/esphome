#pragma once

#include "esphome/components/switch/switch.h"
#include "esphome/components/gree/gree.h"

namespace esphome {
namespace gree {

class GreeTurboSwitch : public switch_::Switch, public Parented<GreeClimate> {
 public:
  void write_state(bool state) override;
};

class GreeLightSwitch : public switch_::Switch, public Parented<GreeClimate> {
 public:
  void write_state(bool state) override;
};

class GreeHealthSwitch : public switch_::Switch, public Parented<GreeClimate> {
 public:
  void write_state(bool state) override;
};

class GreeXfanSwitch : public switch_::Switch, public Parented<GreeClimate> {
 public:
  void write_state(bool state) override;
};

}  // namespace gree
}  // namespace esphome
