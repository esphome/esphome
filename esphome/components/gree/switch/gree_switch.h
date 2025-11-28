#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/gree/gree.h"

namespace esphome {
namespace gree {

class GreeTurboSwitch : public switch_::Switch, public Component, public Parented<GreeClimate> {
 public:
  void setup() override;
  void dump_config() override;
  void write_state(bool state) override;
};

class GreeLightSwitch : public switch_::Switch, public Component, public Parented<GreeClimate> {
 public:
  void setup() override;
  void dump_config() override;
  void write_state(bool state) override;
};

class GreeHealthSwitch : public switch_::Switch, public Component, public Parented<GreeClimate> {
 public:
  void setup() override;
  void dump_config() override;
  void write_state(bool state) override;
};

class GreeXfanSwitch : public switch_::Switch, public Component, public Parented<GreeClimate> {
 public:
  void setup() override;
  void dump_config() override;
  void write_state(bool state) override;
};

}  // namespace gree
}  // namespace esphome
