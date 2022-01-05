#ifdef USE_ARDUINO

#include "wake_on_lan.h"
#include "esphome/core/log.h"
#include "esphome/components/network/ip_address.h"
#include "esphome/components/network/util.h"

namespace esphome {
namespace wake_on_lan{

static const char *const TAG = "wake_on_lan.button";

void WakeOnLanButton::set_macaddr(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f) {
  macaddr[0] = a;
  macaddr[1] = b;
  macaddr[2] = c;
  macaddr[3] = d;
  macaddr[4] = e;
  macaddr[5] = f;
}

void WakeOnLanButton::dump_config() {
  ESP_LOGCONFIG(TAG, "wake_on_lan:");
  ESP_LOGCONFIG(TAG, "  Target MAC address: %02X:%02X:%02X:%02X:%02X:%02X",
    macaddr[0], macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5]);
}

void WakeOnLanButton::press_action() {
  ESP_LOGI(TAG, "Sending Wake On Lan Packet...");
  bool begin_status = false;
  bool end_status = false;
  uint32_t interface = esphome::network::get_ip_address();
  IPAddress interface_ip = IPAddress(interface);
  IPAddress broadcast = IPAddress(255, 255, 255, 255);
  #ifdef USE_ESP8266
  begin_status = this->udp_client_.beginPacketMulticast(
        broadcast,
        9,
        interface_ip,
        128
  );
  #endif
  #ifdef USE_ESP32
  begin_status = this->udp_client_.beginPacket(broadcast, 9);
  #endif

  if (begin_status) {
    uint8_t prefix[6] = {255, 255, 255, 255, 255, 255};
    
    this->udp_client_.write(prefix, 6);
    for (size_t i = 0; i < 16; i++)
    {
      this->udp_client_.write(macaddr, 6);
    }  
    end_status = this->udp_client_.endPacket();
  }
  if (!begin_status || end_status) {
    ESP_LOGE(TAG, "Sending Wake On Lan Packet Failed!");
  }
}

}
}

#endif
