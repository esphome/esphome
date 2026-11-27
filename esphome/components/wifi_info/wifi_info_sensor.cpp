#include "wifi_info_sensor.h"
#if defined(USE_WIFI_CONNECT_STATE_LISTENERS) && defined(USE_SENSOR)
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::wifi_info {

ESPHOME_LOG_TAG(TAG, "wifi_info");

void ChannelWiFiInfo::setup() { wifi::global_wifi_component->add_connect_state_listener(this); }

void ChannelWiFiInfo::dump_config() { LOG_SENSOR("", "Channel", this); }

void ChannelWiFiInfo::on_wifi_connect_state(StringRef ssid, std::span<const uint8_t, 6> bssid) {
  float channel = NAN;
  if (mac_address_is_valid(bssid.data())) {
    const int32_t value = wifi::global_wifi_component->get_wifi_channel();
    if (value > 0)
      channel = value;
  }
  this->publish_state(channel);
}

}  // namespace esphome::wifi_info
#endif
