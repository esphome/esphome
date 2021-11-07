#ifdef USE_ARDUINO

#include "wled_light_effect.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESP32
#include <WiFi.h>
#endif

#ifdef USE_ESP8266
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#endif

namespace esphome {
namespace wled {

// Description of protocols:
// https://github.com/Aircoookie/WLED/wiki/UDP-Realtime-Control
enum Protocol { WLED_NOTIFIER = 0, WARLS = 1, DRGB = 2, DRGBW = 3, DNRGB = 4 };

const int DEFAULT_BLANK_TIME = 1000;

static const char *const TAG = "wled_light_effect";

WLEDLightEffect::WLEDLightEffect(const std::string &name) : AddressableLightEffect(name) {}

void WLEDLightEffect::start() {
  AddressableLightEffect::start();

  blank_at_ = 0;
}

void WLEDLightEffect::stop() {
  AddressableLightEffect::stop();

  if (udp_) {
    udp_->stop();
    udp_.reset();
  }
}

void WLEDLightEffect::blank_all_leds_(light::AddressableLight &it) {
  for (int led = it.size(); led-- > 0;) {
    it[led].set(Color::BLACK);
  }
  it.schedule_show();
}

void WLEDLightEffect::apply(light::AddressableLight &it, const Color &current_color) {
  // Init UDP lazily
  if (!udp_) {
    udp_ = make_unique<WiFiUDP>();

    if (!udp_->begin(port_)) {
      ESP_LOGW(TAG, "Cannot bind WLEDLightEffect to %d.", port_);
      return;
    }
  }

  std::vector<uint8_t> payload;
  while (uint16_t packet_size = udp_->parsePacket()) {
    payload.resize(packet_size);

    if (!udp_->read(&payload[0], payload.size())) {
      continue;
    }

    if (!this->parse_frame_(it, &payload[0], payload.size())) {
      ESP_LOGD(TAG, "Frame: Invalid (size=%zu, first=0x%02X).", payload.size(), payload[0]);
      continue;
    }
  }

  // FIXME: Use roll-over safe arithmetic
  if (blank_at_ < millis()) {
    blank_all_leds_(it);
    blank_at_ = millis() + DEFAULT_BLANK_TIME;
  }
}

bool WLEDLightEffect::parse_frame_(light::AddressableLight &it, const uint8_t *payload, uint16_t size) {
  // At minimum frame needs to have:
  // 1b - protocol
  // 1b - timeout
  if (size < 2) {
    return false;
  }

  uint8_t protocol = payload[0];
  uint8_t timeout = payload[1];

  payload += 2;
  size -= 2;

  switch (protocol) {
    case WLED_NOTIFIER:
      // Hyperion Port
      if (port_ == 19446) {
        if (!parse_drgb_frame_(it, payload, size))
          return false;
      } else {
        if (!parse_notifier_frame_(it, payload, size))
          return false;
      }
      break;

    case WARLS:
      if (!parse_warls_frame_(it, payload, size))
        return false;
      break;

    case DRGB:
      if (!parse_drgb_frame_(it, payload, size))
        return false;
      break;

    case DRGBW:
      if (!parse_drgbw_frame_(it, payload, size))
        return false;
      break;

    case DNRGB:
      if (!parse_dnrgb_frame_(it, payload, size))
        return false;
      break;

    default:
      return false;
  }

  if (timeout == UINT8_MAX) {
    blank_at_ = UINT32_MAX;
  } else if (timeout > 0) {
    blank_at_ = millis() + timeout * 1000;
  } else {
    blank_at_ = millis() + DEFAULT_BLANK_TIME;
  }

  it.schedule_show();
  return true;
}

bool WLEDLightEffect::parse_notifier_frame_(light::AddressableLight &it, const uint8_t *payload, uint16_t size) {
  // Packet needs to be empty
  return size == 0;
}

bool WLEDLightEffect::parse_warls_frame_(light::AddressableLight &it, const uint8_t *payload, uint16_t size) {
  // packet: index, r, g, b
  if ((size % 4) != 0) {
    return false;
  }

  auto count = size / 4;
  auto max_leds = it.size();

  for (; count > 0; count--, payload += 4) {
    uint8_t led = payload[0];
    uint8_t r = payload[1];
    uint8_t g = payload[2];
    uint8_t b = payload[3];

    if (led < max_leds) {
      it[led].set(Color(r, g, b));
    }
  }

  return true;
}

bool WLEDLightEffect::parse_drgb_frame_(light::AddressableLight &it, const uint8_t *payload, uint16_t size) {
  // packet: r, g, b
  if ((size % 3) != 0) {
    return false;
  }

  auto count = size / 3;
  auto max_leds = it.size();

  for (uint16_t led = 0; led < count; ++led, payload += 3) {
    uint8_t r = payload[0];
    uint8_t g = payload[1];
    uint8_t b = payload[2];

    if (led < max_leds) {
      it[led].set(Color(r, g, b));
    }
  }

  return true;
}

bool WLEDLightEffect::parse_drgbw_frame_(light::AddressableLight &it, const uint8_t *payload, uint16_t size) {
  // packet: r, g, b, w
  if ((size % 4) != 0) {
    return false;
  }

  auto count = size / 4;
  auto max_leds = it.size();

  for (uint16_t led = 0; led < count; ++led, payload += 4) {
    uint8_t r = payload[0];
    uint8_t g = payload[1];
    uint8_t b = payload[2];
    uint8_t w = payload[3];

    if (led < max_leds) {
      it[led].set(Color(r, g, b, w));
    }
  }

  return true;
}

bool WLEDLightEffect::parse_dnrgb_frame_(light::AddressableLight &it, const uint8_t *payload, uint16_t size) {
  // offset: high, low
  if (size < 2) {
    return false;
  }

  uint16_t led = (uint16_t(payload[0]) << 8) + payload[1];
  payload += 2;
  size -= 2;

  // packet: r, g, b
  if ((size % 3) != 0) {
    return false;
  }

  auto count = size / 3;
  auto max_leds = it.size();

  for (; count > 0; count--, payload += 3, led++) {
    uint8_t r = payload[0];
    uint8_t g = payload[1];
    uint8_t b = payload[2];

    if (led < max_leds) {
      it[led].set(Color(r, g, b));
    }
  }

  return true;
}

}  // namespace wled
}  // namespace esphome

#endif  // USE_ARDUINO
