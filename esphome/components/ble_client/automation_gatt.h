// Neutral twins of the shared ble_client automations. Class names, namespace,
// and codegen-visible signatures are IDENTICAL to automation.h so generated
// main.cpp compiles against whichever engine the build gates in; only the
// internals differ (client callbacks and the neutral node interface instead
// of raw gattc events). The Bluedroid-security automations (passkey, numeric
// comparison, remove bond) have no neutral equivalent and stay esp32-only.

#pragma once

#include "esphome/core/defines.h"

#if defined(USE_BLE_GATT_CLIENT) && !defined(USE_BLE_CLIENT_LEGACY_ENGINE)

#include <tuple>

#include "ble_client_gatt.h"
#include "esphome/core/automation.h"

namespace esphome::ble_client {

class BLEClientConnectTrigger final : public Trigger<> {
 public:
  explicit BLEClientConnectTrigger(BLEClient *parent) {
    parent->add_on_connect_callback([this]() { this->trigger(); });
  }
};

class BLEClientDisconnectTrigger final : public Trigger<> {
 public:
  explicit BLEClientDisconnectTrigger(BLEClient *parent) {
    // Fires only after a completed connection (never for failed attempts),
    // matching the legacy CLOSE_EVT semantics.
    parent->add_on_disconnect_callback([this]() { this->trigger(); });
  }
};

template<typename... Ts> class BLEClientConnectAction final : public Action<Ts...> {
 public:
  BLEClientConnectAction(BLEClient *ble_client) {
    ble_client_ = ble_client;
    ble_client->add_on_connect_callback([this]() {
      if (this->num_running_ != 0)
        this->play_next_tuple_(this->var_);
    });
    // A connect attempt that dies (or a later disconnect) terminates the
    // chain, mirroring the legacy DISCONNECT_EVT handling.
    ble_client->add_on_connect_failed_callback([this]() {
      if (this->num_running_ != 0)
        this->stop_complex();
    });
    ble_client->add_on_disconnect_callback([this]() {
      if (this->num_running_ != 0)
        this->stop_complex();
    });
  }

  // not used since we override play_complex_
  void play(const Ts &...x) override {}

  void play_complex(const Ts &...x) override {
    // it makes no sense to have multiple instances of this running at the
    // same time; cancel a re-trigger while still running.
    if (this->num_running_ != 0) {
      this->stop_complex();
      return;
    }
    this->num_running_++;
    if (this->ble_client_->connected()) {
      this->play_next_(x...);
    } else {
      this->var_ = std::make_tuple(x...);
      // No-op while already connecting; the callback resolves the wait.
      this->ble_client_->connect();
    }
  }

 private:
  BLEClient *ble_client_;
  std::tuple<Ts...> var_{};
};

template<typename... Ts> class BLEClientDisconnectAction final : public Action<Ts...> {
 public:
  BLEClientDisconnectAction(BLEClient *ble_client) {
    ble_client_ = ble_client;
    // Both terminal outcomes resolve the wait: a completed teardown and a
    // connect attempt that died on the way down.
    ble_client->add_on_disconnect_callback([this]() {
      if (this->num_running_ != 0)
        this->play_next_tuple_(this->var_);
    });
    ble_client->add_on_connect_failed_callback([this]() {
      if (this->num_running_ != 0)
        this->play_next_tuple_(this->var_);
    });
  }

  // not used since we override play_complex_
  void play(const Ts &...x) override {}

  void play_complex(const Ts &...x) override {
    this->num_running_++;
    if (this->ble_client_->idle()) {
      this->play_next_(x...);
    } else {
      this->var_ = std::make_tuple(x...);
      this->ble_client_->disconnect();
    }
  }

 private:
  BLEClient *ble_client_;
  std::tuple<Ts...> var_{};
};

}  // namespace esphome::ble_client

#endif  // USE_BLE_GATT_CLIENT && !USE_BLE_CLIENT_LEGACY_ENGINE
