#pragma once

#include "uart.h"
#include "esphome/core/automation.h"

#include <vector>

namespace esphome::uart {

template<typename... Ts> class UARTWriteAction final : public Action<Ts...>, public Parented<UARTComponent> {
  TEMPLATABLE_BYTES(data)

  void play(const Ts &...x) override {
    if (this->data_.is_static()) {
      this->parent_->write_array_progmem(this->data_.data(), this->data_.size());
    } else {
      this->parent_->write_array(this->data_.value(x...));
    }
  }
};

}  // namespace esphome::uart
