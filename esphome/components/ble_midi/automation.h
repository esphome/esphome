#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "esphome/core/automation.h"

#include "ble_midi.h"

namespace esphome::ble_midi {

template<typename... Ts> class SendSysexAction final : public Action<Ts...>, public Parented<BLEMidi> {
  TEMPLATABLE_BYTES(payload)

 public:
  void play(const Ts &...x) override {
    this->payload_.template visit<MAX_TX_MIDI_BYTES>(
        [this](const uint8_t *data, size_t len) { this->parent_->send_sysex(data, len); }, x...);
  }
};

template<typename... Ts> class SendRawAction final : public Action<Ts...>, public Parented<BLEMidi> {
  TEMPLATABLE_BYTES(data)

 public:
  void play(const Ts &...x) override {
    this->data_.template visit<MAX_TX_MIDI_BYTES>(
        [this](const uint8_t *data, size_t len) { this->parent_->send_raw(data, len); }, x...);
  }
};

}  // namespace esphome::ble_midi

#endif  // USE_ESP32
