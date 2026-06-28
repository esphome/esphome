#pragma once
#ifdef USE_ZEPHYR
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include <zephyr/bluetooth/conn.h>
#include "esphome/core/automation.h"

#ifdef USE_OTA_STATE_LISTENER
#include "esphome/components/ota/ota_backend.h"
#endif

namespace esphome::zephyr_ble_server {

class BLEServer : public Component
#ifdef USE_OTA_STATE_LISTENER
    ,
                  public ota::OTAGlobalStateListener
#endif
{
 public:
  void setup() override;
  void dump_config() override;
  template<typename F> void add_passkey_callback(F &&callback) { this->passkey_cb_.add(std::forward<F>(callback)); }
  void numeric_comparison_reply(bool accept);

  // Link-gated advertising control (the BLE analogue of wifi.enable/disable). The
  // node-soil ble_policy drives this off the hub link: a hub-linked node stops
  // advertising (no commissioning/OTA surface), and an operator "wake" re-enables it
  // for a bounded window. enabled=false also drops an active central so "off" fully
  // closes the surface (disconnect-on-stop) -- EXCEPT while an OTA is in flight, which
  // is never interrupted. advertising_wanted_ latches the intent so the disconnect
  // handler (which re-advertises) honours it instead of silently coming back up.
  void set_advertising_enabled(bool enabled);
  bool is_advertising_wanted() const { return this->advertising_wanted_; }

#ifdef USE_OTA_STATE_LISTENER
  // Track OTA in/out so disconnect-on-stop never tears down a connection mid-update.
  void on_ota_global_state(ota::OTAState state, float progress, uint8_t error, ota::OTAComponent *comp) override;
#endif

 protected:
  static void connected(bt_conn *conn, uint8_t err);
  static void disconnected(bt_conn *conn, uint8_t reason);
  static void auth_passkey_confirm(bt_conn *conn, unsigned int passkey);
  bt_conn *conn_{};
  CallbackManager<void(uint32_t)> passkey_cb_;
  // Whether advertising should currently be up. Default true so boot behaviour is
  // unchanged (advertise at setup); the policy lowers it once the node is hub-linked.
  bool advertising_wanted_{true};
  // True between OTA_STARTED and OTA_COMPLETED/ERROR/ABORT; suppresses the
  // disconnect-on-stop so a remote firmware update is never cut off.
  bool ota_in_progress_{false};
};

template<typename... Ts> class BLENumericComparisonReplyAction : public Action<Ts...> {
 public:
  explicit BLENumericComparisonReplyAction(BLEServer *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(bool, accept)

  void play(const Ts &...x) override { this->parent_->numeric_comparison_reply(this->accept_.value(x...)); }

 protected:
  BLEServer *parent_;
};

template<typename... Ts> class BLEStartAdvertisingAction : public Action<Ts...> {
 public:
  explicit BLEStartAdvertisingAction(BLEServer *parent) : parent_(parent) {}
  void play(const Ts &...x) override { this->parent_->set_advertising_enabled(true); }

 protected:
  BLEServer *parent_;
};

template<typename... Ts> class BLEStopAdvertisingAction : public Action<Ts...> {
 public:
  explicit BLEStopAdvertisingAction(BLEServer *parent) : parent_(parent) {}
  void play(const Ts &...x) override { this->parent_->set_advertising_enabled(false); }

 protected:
  BLEServer *parent_;
};

}  // namespace esphome::zephyr_ble_server
#endif
