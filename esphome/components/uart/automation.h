#pragma once

#include "uart.h"
#include "esphome/core/automation.h"

#include <vector>

#ifdef USE_ZEPHYR_UART_EMULATION
#include "esphome/components/zephyr/uart_emulator.h"
#endif

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

#ifdef USE_ZEPHYR_UART_EMULATION
// Injects data directly into the device's RX path via a ZephyrUartEmulator, instead of
// writing out TX -- used to model an emulated peripheral initiating an exchange unprompted.
template<typename... Ts>
class UARTEmulatorPushRxAction final : public Action<Ts...>, public Parented<zephyr::ZephyrUartEmulator> {
  TEMPLATABLE_BYTES(data)

  void play(const Ts &...x) override {
    this->data_.visit([this](const uint8_t *data, size_t len) { this->parent_->push_rx(data, len); }, x...);
  }
};
#endif

}  // namespace esphome::uart
