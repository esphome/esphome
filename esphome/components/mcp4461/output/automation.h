#pragma once

#include "esphome/core/automation.h"
#include "mcp4461_output.h"

namespace esphome::mcp4461 {

template<typename... Ts> class WiperIncreaseAction : public Action<Ts...> {
 public:
  explicit WiperIncreaseAction(Mcp4461Wiper *wiper) : wiper_(wiper) {}
  void play(Ts... x) override { this->wiper_->increase_wiper(); }

 protected:
  Mcp4461Wiper *wiper_;
};

template<typename... Ts> class WiperDecreaseAction : public Action<Ts...> {
 public:
  explicit WiperDecreaseAction(Mcp4461Wiper *wiper) : wiper_(wiper) {}
  void play(Ts... x) override { this->wiper_->decrease_wiper(); }

 protected:
  Mcp4461Wiper *wiper_;
};

// Persist the current level to the chip's nonvolatile register immediately — useful with
// nonvolatile: false to persist only at deliberate moments (e.g. on a button press), or to
// bypass the stability delay of the automatic mirroring.
template<typename... Ts> class WiperStoreNonvolatileAction : public Action<Ts...> {
 public:
  explicit WiperStoreNonvolatileAction(Mcp4461Wiper *wiper) : wiper_(wiper) {}
  void play(Ts... x) override { this->wiper_->store_nonvolatile(); }

 protected:
  Mcp4461Wiper *wiper_;
};

template<typename... Ts> class WiperSetTerminalAction : public Action<Ts...> {
 public:
  WiperSetTerminalAction(Mcp4461Wiper *wiper, char terminal, bool enable)
      : wiper_(wiper), terminal_(terminal), enable_(enable) {}
  void play(Ts... x) override {
    if (this->enable_) {
      this->wiper_->enable_terminal(this->terminal_);
    } else {
      this->wiper_->disable_terminal(this->terminal_);
    }
  }

 protected:
  Mcp4461Wiper *wiper_;
  char terminal_;
  bool enable_;
};

}  // namespace esphome::mcp4461
