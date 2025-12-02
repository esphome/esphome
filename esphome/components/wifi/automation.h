#pragma once

#include "esphome/core/defines.h"
#ifdef USE_WIFI
#include "wifi_component.h"

namespace esphome::wifi {

template<typename... Ts> class WiFiConnectedCondition : public Condition<Ts...> {
 public:
  bool check(const Ts &...x) override { return global_wifi_component->is_connected(); }
};

template<typename... Ts> class WiFiEnabledCondition : public Condition<Ts...> {
 public:
  bool check(const Ts &...x) override { return !global_wifi_component->is_disabled(); }
};

template<typename... Ts> class WiFiAPActiveCondition : public Condition<Ts...> {
 public:
  bool check(const Ts &...x) override { return global_wifi_component->is_ap_active(); }
};

template<typename... Ts> class WiFiEnableAction : public Action<Ts...> {
 public:
  void play(const Ts &...x) override { global_wifi_component->enable(); }
};

template<typename... Ts> class WiFiDisableAction : public Action<Ts...> {
 public:
  void play(const Ts &...x) override { global_wifi_component->disable(); }
};

template<typename... Ts> class WiFiConfigureAction : public Action<Ts...>, public Component {
 public:
  TEMPLATABLE_VALUE(std::string, ssid)
  TEMPLATABLE_VALUE(std::string, password)
  TEMPLATABLE_VALUE(bool, save)
  TEMPLATABLE_VALUE(uint32_t, connection_timeout)

  void play(const Ts &...x) override {
    auto ssid = this->ssid_.value(x...);
    auto password = this->password_.value(x...);
    // Avoid multiple calls
    if (this->connecting_)
      return;
    // If already connected to the same AP, do nothing
    if (global_wifi_component->wifi_ssid() == ssid) {
      // Callback to notify the user that the connection was successful
      this->connect_trigger_->trigger();
      return;
    }
    // Create a new WiFiAP object with the new SSID and password
    this->new_sta_.set_ssid(ssid);
    this->new_sta_.set_password(password);
    // Save the current STA
    this->old_sta_ = global_wifi_component->get_sta();
    // Disable WiFi
    global_wifi_component->disable();
    // Set the state to connecting
    this->connecting_ = true;
    // Store the new STA so once the WiFi is enabled, it will connect to it
    // This is necessary because the WiFiComponent will raise an error and fallback to the saved STA
    // if trying to connect to a new STA while already connected to another one
    if (this->save_.value(x...)) {
      global_wifi_component->save_wifi_sta(new_sta_.get_ssid(), new_sta_.get_password());
    } else {
      global_wifi_component->set_sta(new_sta_);
    }
    // Enable WiFi
    global_wifi_component->enable();
    // Set timeout for the connection
    this->set_timeout("wifi-connect-timeout", this->connection_timeout_.value(x...), [this, x...]() {
      // If the timeout is reached, stop connecting and revert to the old AP
      global_wifi_component->disable();
      global_wifi_component->save_wifi_sta(old_sta_.get_ssid(), old_sta_.get_password());
      global_wifi_component->enable();
      // Start a timeout for the fallback if the connection to the old AP fails
      this->set_timeout("wifi-fallback-timeout", this->connection_timeout_.value(x...), [this]() {
        this->connecting_ = false;
        this->error_trigger_->trigger();
      });
    });
  }

  Trigger<> *get_connect_trigger() const { return this->connect_trigger_; }
  Trigger<> *get_error_trigger() const { return this->error_trigger_; }

  void loop() override {
    if (!this->connecting_)
      return;
    if (global_wifi_component->is_connected()) {
      // The WiFi is connected, stop the timeout and reset the connecting flag
      this->cancel_timeout("wifi-connect-timeout");
      this->cancel_timeout("wifi-fallback-timeout");
      this->connecting_ = false;
      if (global_wifi_component->wifi_ssid() == this->new_sta_.get_ssid()) {
        // Callback to notify the user that the connection was successful
        this->connect_trigger_->trigger();
      } else {
        // Callback to notify the user that the connection failed
        this->error_trigger_->trigger();
      }
    }
  }

 protected:
  bool connecting_{false};
  WiFiAP new_sta_;
  WiFiAP old_sta_;
  Trigger<> *connect_trigger_{new Trigger<>()};
  Trigger<> *error_trigger_{new Trigger<>()};
};

}  // namespace esphome::wifi
#endif
