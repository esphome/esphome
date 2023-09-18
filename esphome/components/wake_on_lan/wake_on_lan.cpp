#ifdef USE_ARDUINO

#include "wake_on_lan.h"
#include "esphome/core/log.h"
#include "esphome/components/network/ip_address.h"
#include "esphome/components/network/util.h"

namespace esphome {
namespace wake_on_lan {

static const char *const TAG = "wake_on_lan.button";
static const uint8_t PREFIX[6] = {255, 255, 255, 255, 255, 255};

void WakeOnLanButton::set_macaddr(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f) {
  macaddr_[0] = a;
  macaddr_[1] = b;
  macaddr_[2] = c;
  macaddr_[3] = d;
  macaddr_[4] = e;
  macaddr_[5] = f;
}

void WakeOnLanButton::dump_config() {
  LOG_BUTTON("", "Wake-on-LAN Button", this);
  ESP_LOGCONFIG(TAG, "  Target MAC address: %02X:%02X:%02X:%02X:%02X:%02X", macaddr_[0], macaddr_[1], macaddr_[2],
                macaddr_[3], macaddr_[4], macaddr_[5]);
}

void WakeOnLanButton::press_action() {
  ESP_LOGI(TAG, "Sending Wake-on-LAN Packet...");
  bool begin_status = false;
  bool end_status = false;
  IPAddress broadcast = IPAddress(255, 255, 255, 255);
#ifdef USE_ESP8266
  begin_status = this->udp_client_.beginPacketMulticast(broadcast, 9,
                                                        IPAddress((ip_addr_t) esphome::network::get_ip_address()), 128);
#endif
#ifdef USE_ESP32
  begin_status = this->udp_client_.beginPacket(broadcast, 9);
#endif

  if (begin_status) {
    this->udp_client_.write(PREFIX, 6);
    for (size_t i = 0; i < 16; i++) {
      this->udp_client_.write(macaddr_, 6);
    }
    end_status = this->udp_client_.endPacket();
  }
  if (!begin_status || end_status) {
    ESP_LOGE(TAG, "Sending Wake-on-LAN Packet Failed!");
  }
}

}  // namespace wake_on_lan
}  // namespace esphome

#endif
