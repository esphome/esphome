#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "esphome/core/automation.h"

#include "ble_midi.h"

namespace esphome::ble_midi {

template<typename... Ts> class SendSysexAction final : public Action<Ts...>, public Parented<BLEMidi> {
  TEMPLATABLE_BYTES(bytes)

 public:
  void play(const Ts &...x) override {
    this->bytes_.visit([this](const uint8_t *data, size_t len) { this->parent_->send_sysex(data, len); }, x...);
  }
};

template<typename... Ts> class SendRawAction final : public Action<Ts...>, public Parented<BLEMidi> {
  TEMPLATABLE_BYTES(bytes)

 public:
  void play(const Ts &...x) override {
    this->bytes_.visit([this](const uint8_t *data, size_t len) { this->parent_->send_raw(data, len); }, x...);
  }
};

}  // namespace esphome::ble_midi

#endif  // USE_ESP32
