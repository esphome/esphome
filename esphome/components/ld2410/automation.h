#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "ld2410.h"

namespace esphome {
namespace ld2410 {

template<typename... Ts> class BluetoothPasswordSetAction : public Action<Ts...> {
 public:
  explicit BluetoothPasswordSetAction(LD2410Component *ld2410_comp) : ld2410_comp_(ld2410_comp) {}
  TEMPLATABLE_VALUE(std::string, password)

  void play(Ts... x) override { this->ld2410_comp_->set_bluetooth_password(this->password_.value(x...)); }

 protected:
  LD2410Component *ld2410_comp_;
};

}  // namespace ld2410
}  // namespace esphome
