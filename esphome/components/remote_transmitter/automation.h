#pragma once

#include "esphome/components/remote_transmitter/remote_transmitter.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace remote_transmitter {

template<typename... Ts> class DigitalWriteAction : public Action<Ts...>, public Parented<RemoteTransmitterComponent> {
 public:
  TEMPLATABLE_VALUE(bool, value)
  void play(const Ts &...x) override { this->parent_->digital_write(this->value_.value(x...)); }
};

}  // namespace remote_transmitter
}  // namespace esphome
