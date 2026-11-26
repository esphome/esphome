#include "ethernet_component.h"

#ifdef USE_ETHERNET

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::ethernet {

EthernetComponent *global_eth_component;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

EthernetComponent::EthernetComponent() { global_eth_component = this; }

#if defined(USE_ESP32) || defined(USE_HOST)
ESPHOME_LOG_TAG(TAG, "ethernet");

void EthernetComponent::loop() {
  if (this->pending_enable_ && !this->driver_stopped_.load(std::memory_order_acquire)) {
    return;
  }
  const uint32_t now = App.get_loop_component_start_time();

#ifdef USE_ETHERNET_IP_STATE_LISTENERS
  // The IP events arrive on the event task; the listeners publish states, so notify them here
  if (this->ip_state_pending_.load(std::memory_order_acquire) != 0) {
    this->ip_state_pending_.store(0, std::memory_order_relaxed);
    this->notify_ip_state_listeners_();
  }
#endif

  const bool link_up = this->link_up_.load(std::memory_order_relaxed);
  if (link_up && !this->link_was_up_) {
    // Give DHCP a full window after the link comes up before the timeout below restarts it
    this->connect_begin_ = now;
  }
  this->link_was_up_ = link_up;

  switch (this->state_) {
    case EthernetComponentState::STOPPED:
      // A failed driver start can post START before its cleanup posts STOP.
      if (!this->disabled_ && this->started_) {
        ESP_LOGI(TAG, "Starting connection");
        this->state_ = EthernetComponentState::CONNECTING;
        this->start_connect_();
      }
      break;
    case EthernetComponentState::CONNECTING:
      if (!this->started_) {
        ESP_LOGI(TAG, "Stopped connection");
        this->state_ = EthernetComponentState::STOPPED;
      } else if (this->connected_) {
        // connection established
        ESP_LOGI(TAG, "Connected");
        this->state_ = EthernetComponentState::CONNECTED;

        this->dump_connect_params_();
        this->status_clear_warning();
#ifdef USE_ETHERNET_CONNECT_TRIGGER
        this->connect_trigger_.trigger();
#endif
        this->notify_connect_state_listeners_(true);
      } else if (now - this->connect_begin_ > 15000) {
        if (link_up) {
          ESP_LOGW(TAG, "Connecting failed; reconnecting");
          this->start_connect_();
        } else {
          // Restarting DHCP without a link only empties lwIP's DNS servers (espressif/esp-idf#19082);
          // the DHCP client already running resumes when the link comes up.
          ESP_LOGD(TAG, "Connecting failed: no link");
          this->connect_begin_ = now;
        }
      }
      break;
    case EthernetComponentState::CONNECTED:
      if (!this->started_) {
        ESP_LOGI(TAG, "Stopped connection");
        this->state_ = EthernetComponentState::STOPPED;
#ifdef USE_ETHERNET_DISCONNECT_TRIGGER
        this->disconnect_trigger_.trigger();
#endif
        this->notify_connect_state_listeners_(false);
      } else if (!this->connected_) {
        ESP_LOGW(TAG, "Connection lost; reconnecting");
        this->state_ = EthernetComponentState::CONNECTING;
        this->start_connect_();
#ifdef USE_ETHERNET_DISCONNECT_TRIGGER
        this->disconnect_trigger_.trigger();
#endif
        this->notify_connect_state_listeners_(false);
      } else {
        this->finish_connect_();
        // When connected and stable, disable the loop to save CPU cycles
        this->disable_loop();
      }
      break;
  }

  // Process the stopped connection and its automation before consuming the restart intent.
  // An on_disconnect action may cancel the pending enable or start the driver itself.
  if (this->pending_enable_ && this->driver_stopped_.load(std::memory_order_acquire)) {
    this->pending_enable_(this);
  }
}

void EthernetComponent::enable() {
  if (!this->disabled_)
    return;
  if (!this->driver_stopped_.load(std::memory_order_acquire) || this->state_ != EthernetComponentState::STOPPED) {
    this->pending_enable_ = [](EthernetComponent *component) { component->enable(); };
    this->enable_loop();
    return;
  }
  this->pending_enable_ = nullptr;

  ESP_LOGD(TAG, "Enabling");
  this->ethernet_lazy_init_();
  if (!this->ethernet_initialized_) {
    ESP_LOGE(TAG, "Cannot enable - init failed");
    return;
  }
  this->state_ = EthernetComponentState::STOPPED;
  this->connected_ = false;
  this->started_ = false;
  this->driver_stopped_.store(false, std::memory_order_release);
  if (!this->start_driver_()) {
    // A failed start may leave the driver's state changed. Only STOP delivery permits another start.
    this->stop_driver_(DriverStopReason::DRIVER_STOP_REASON_FAILED_START);
    return;
  }
  this->disabled_ = false;
  // The ETH_EVENT_START handler will set started_=true; the loop state machine
  // will then drive the STOPPED -> CONNECTING -> CONNECTED transitions.
  this->enable_loop();
}

void EthernetComponent::disable() {
  this->pending_enable_ = nullptr;
  if (this->disabled_)
    return;

  ESP_LOGD(TAG, "Disabling");
  if (!this->stop_driver_(DriverStopReason::DRIVER_STOP_REASON_DISABLE))
    return;
  this->disabled_ = true;
  // ETH_EVENT_STOP will clear started_; loop() will transition to STOPPED.
}

#endif

#ifdef USE_ETHERNET_MANUAL_IP
ManualIP load_manual_ip() {
  uint8_t b[ETHERNET_MANUAL_IP_SIZE];
  progmem_memcpy(b, ETHERNET_MANUAL_IP, sizeof(b));
  auto ip = [&b](size_t i) { return network::IPAddress(b[i], b[i + 1], b[i + 2], b[i + 3]); };
  return ManualIP{ip(0), ip(4), ip(8), ip(12), ip(16)};
}
#endif

#ifdef USE_ETHERNET_IP_STATE_LISTENERS
void EthernetComponent::notify_ip_state_listeners_() {
  auto ips = this->get_ip_addresses();
  auto dns1 = this->get_dns_address(0);
  auto dns2 = this->get_dns_address(1);
  for (auto *listener : this->ip_state_listeners_) {
    listener->on_ip_state(ips, dns1, dns2);
  }
}
#endif  // USE_ETHERNET_IP_STATE_LISTENERS

}  // namespace esphome::ethernet

#endif  // USE_ETHERNET
