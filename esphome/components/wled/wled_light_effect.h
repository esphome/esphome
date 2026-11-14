#pragma once

#include "esphome/core/defines.h"
#ifdef USE_NETWORK

#include "esphome/components/light/addressable_light_effect.h"
#include "esphome/components/socket/socket.h"

#include <memory>

namespace esphome::wled {

class WLEDLightEffect : public light::AddressableLightEffect {
 public:
  WLEDLightEffect(const char *name);

  void start() override;
  void stop() override;
  void apply(light::AddressableLight &it, const Color &current_color) override;
  void set_port(uint16_t port) { this->port_ = port; }
  void set_sync_group_mask(uint8_t mask) { this->sync_group_mask_ = mask; }
  void set_blank_on_start(bool blank) { this->blank_on_start_ = blank; }

 protected:
  void open_socket_();
  void blank_all_leds_(light::AddressableLight &it);
  bool parse_frame_(light::AddressableLight &it, const uint8_t *payload, uint16_t size);
  bool parse_notifier_frame_(light::AddressableLight &it, const uint8_t *payload, uint16_t size);
  bool parse_warls_frame_(light::AddressableLight &it, const uint8_t *payload, uint16_t size);
  bool parse_drgb_frame_(light::AddressableLight &it, const uint8_t *payload, uint16_t size);
  bool parse_drgbw_frame_(light::AddressableLight &it, const uint8_t *payload, uint16_t size);
  bool parse_dnrgb_frame_(light::AddressableLight &it, const uint8_t *payload, uint16_t size);

  uint16_t port_{0};
  std::unique_ptr<socket::UDPSocket> socket_;
  optional<uint32_t> blank_start_{};
  uint32_t blank_timeout_{0};
  uint32_t dropped_{0};
  uint8_t sync_group_mask_{0};
  bool blank_on_start_{true};
  bool socket_failed_{false};  // until the next start(), so a bind error is not retried every loop
};

}  // namespace esphome::wled

#endif  // USE_NETWORK
