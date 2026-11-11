#include "wled_light_effect.h"
#ifdef USE_NETWORK

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::wled {

// Description of protocols:
// https://github.com/Aircoookie/WLED/wiki/UDP-Realtime-Control
enum Protocol { WLED_NOTIFIER = 0, WARLS = 1, DRGB = 2, DRGBW = 3, DNRGB = 4 };

constexpr uint32_t DEFAULT_BLANK_TIME = 1000;
// The largest UDP payload in one Ethernet frame, enough for a full 490 LED DRGB frame
constexpr size_t MAX_FRAME_SIZE = 1472;

ESPHOME_LOG_TAG(TAG, "wled_light_effect");

WLEDLightEffect::WLEDLightEffect(const char *name) : AddressableLightEffect(name) {}

void WLEDLightEffect::start() {
  AddressableLightEffect::start();

  if (this->blank_on_start_) {
    this->blank_start_ = millis();
    this->blank_timeout_ = 0;
  } else {
    this->blank_start_.reset();
  }
  this->socket_failed_ = false;
}

void WLEDLightEffect::open_socket_() {
  this->socket_ = socket::socket_ip_udp(IPPROTO_IP);
  if (this->socket_ == nullptr) {
    ESP_LOGW(TAG, "Could not create socket: errno %d", errno);
    this->socket_failed_ = true;
    return;
  }
  // No SO_REUSEADDR: a second effect on the same port would take its frames, so let its bind fail instead
  struct sockaddr_storage server{};
  socklen_t server_len = socket::set_sockaddr_any((struct sockaddr *) &server, sizeof(server), this->port_);
  if (this->socket_->setblocking(false) != 0 || server_len == 0 ||
      this->socket_->bind((struct sockaddr *) &server, server_len) != 0) {
    ESP_LOGW(TAG, "Cannot bind to port %u: errno %d", static_cast<unsigned>(this->port_), errno);
    this->socket_.reset();
    this->socket_failed_ = true;
  }
}

void WLEDLightEffect::stop() {
  AddressableLightEffect::stop();

  this->socket_.reset();
}

void WLEDLightEffect::blank_all_leds_(light::AddressableLight &it) {
  for (int led = it.size(); led-- > 0;) {
    it[led].set(Color::BLACK);
  }
  it.schedule_show();
}

void WLEDLightEffect::apply(light::AddressableLight &it, const Color &current_color) {
  // Not before setup completes: a restored effect starts, and setup can run loops, before the network stack is up
  if (this->socket_ == nullptr && !this->socket_failed_ && App.is_setup_complete())
    this->open_socket_();
  if (this->socket_ != nullptr) {
    uint8_t payload[MAX_FRAME_SIZE];
    ssize_t len;
    while ((len = this->socket_->read(payload, sizeof(payload))) > 0) {
      if (!this->parse_frame_(it, payload, static_cast<uint16_t>(len))) {
        ESP_LOGD(TAG, "Frame: Invalid (size=%d, first=0x%02X).", static_cast<int>(len), payload[0]);
      }
    }
  }

  if (this->blank_start_.has_value() && millis() - *this->blank_start_ >= this->blank_timeout_) {
    blank_all_leds_(it);
    this->blank_start_ = millis();
    this->blank_timeout_ = DEFAULT_BLANK_TIME;
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
        if (!parse_notifier_frame_(it, payload, size)) {
          return false;
        } else {
          timeout = UINT8_MAX;
        }
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
    this->blank_start_.reset();
  } else if (timeout > 0) {
    this->blank_start_ = millis();
    this->blank_timeout_ = timeout * 1000;
  } else {
    this->blank_start_ = millis();
    this->blank_timeout_ = DEFAULT_BLANK_TIME;
  }

  it.schedule_show();
  return true;
}

bool WLEDLightEffect::parse_notifier_frame_(light::AddressableLight &it, const uint8_t *payload, uint16_t size) {
  // Receive at least RGBW and Brightness for all LEDs from WLED Sync Notification
  // https://kno.wled.ge/interfaces/udp-notifier/
  // https://github.com/Aircoookie/WLED/blob/main/wled00/udp.cpp

  if (size <= 34) {
    return false;
  }

  uint8_t payload_sync_group_mask = payload[34];

  if (this->sync_group_mask_ && !(payload_sync_group_mask & this->sync_group_mask_)) {
    ESP_LOGD(TAG, "sync group mask does not match");
    return false;
  }

  uint8_t bri = payload[0];
  uint8_t r = esp_scale8(payload[1], bri);
  uint8_t g = esp_scale8(payload[2], bri);
  uint8_t b = esp_scale8(payload[3], bri);
  uint8_t w = esp_scale8(payload[8], bri);

  for (auto &&led : it) {
    led.set(Color(r, g, b, w));
  }

  return true;
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

}  // namespace esphome::wled

#endif  // USE_NETWORK
