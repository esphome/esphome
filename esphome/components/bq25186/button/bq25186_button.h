#pragma once

#include "esphome/components/button/button.h"
#include "../bq25186.h"

namespace esphome::bq25186 {

class BQ25186SoftwareResetButton : public button::Button, public Parented<BQ25186Component> {
 public:
  BQ25186SoftwareResetButton() = default;

 protected:
  void press_action() override;
};

class BQ25186ShutdownButton : public button::Button, public Parented<BQ25186Component> {
 public:
  BQ25186ShutdownButton() = default;

 protected:
  void press_action() override;
};

class BQ25186ShipModeButton : public button::Button, public Parented<BQ25186Component> {
 public:
  BQ25186ShipModeButton() = default;

 protected:
  void press_action() override;
};

class BQ25186HardwareResetButton : public button::Button, public Parented<BQ25186Component> {
 public:
  BQ25186HardwareResetButton() = default;

 protected:
  void press_action() override;
};

}  // namespace esphome::bq25186
