#pragma once

#include "esphome/components/button/button.h"
#include "../ld2420.h"

namespace esphome {
namespace ld2420 {

class LD2420ApplyConfigButton : public button::Button, public Parented<LD2420Component> {
 public:
  LD2420ApplyConfigButton() = default;

 protected:
  void press_action() override;
};

class LD2420RevertConfigButton : public button::Button, public Parented<LD2420Component> {
 public:
  LD2420RevertConfigButton() = default;

 protected:
  void press_action() override;
};

class LD2420RestartModuleButton : public button::Button, public Parented<LD2420Component> {
 public:
  LD2420RestartModuleButton() = default;

 protected:
  void press_action() override;
};

class LD2420FactoryResetButton : public button::Button, public Parented<LD2420Component> {
 public:
  LD2420FactoryResetButton() = default;

 protected:
  void press_action() override;
};

}  // namespace ld2420
}  // namespace esphome
