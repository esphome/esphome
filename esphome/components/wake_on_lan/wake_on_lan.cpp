
#include "wake_on_lan.h"
#include "esphome/core/log.h"
#include "esphome/components/network/ip_address.h"
#include "esphome/components/network/util.h"
#include "esphome/components/socket/socket.h"
#include <cstring>

namespace esphome {
namespace wake_on_lan {

static const char *const TAG = "wake_on_lan.button";

void WakeOnLanButton::set_macaddr(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f) {
  macaddr_[0] = a;
  macaddr_[1] = b;
  macaddr_[2] = c;
  macaddr_[3] = d;
  macaddr_[4] = e;
  macaddr_[5] = f;
}

void WakeOnLanButton::dump_config() {
  ESP_LOGCONFIG(TAG, "wake_on_lan:");
  ESP_LOGCONFIG(TAG, "  Target MAC address: %02X:%02X:%02X:%02X:%02X:%02X", macaddr_[0], macaddr_[1], macaddr_[2],
                macaddr_[3], macaddr_[4], macaddr_[5]);
}

void WakeOnLanButton::press_action() {
  ESP_LOGI(TAG, "Sending Wake On Lan Packet...");
  std::unique_ptr<esphome::socket::Socket> sd = esphome::socket::socket(AF_INET, SOCK_DGRAM, 0);
  if (sd < nullptr) {
    ESP_LOGE(TAG, "Failed to open datagram socket!");
    return;
  }
  sockaddr_in group_sock;
  /*
   * Initialize the group sockaddr structure with a
   * group address of 225.1.1.1 and port 5555.
   */
  memset((char *) &group_sock, 0, sizeof(group_sock));
  group_sock.sin_family = AF_INET;
  group_sock.sin_addr.s_addr = inet_addr("255.255.255.255");
  group_sock.sin_port = htons(5555);

  in_addr local_interface;
  /*
   * Set local interface for outbound multicast datagrams.
   * The IP address specified must be associated with a local,
   * multicast-capable interface.
   */
  local_interface.s_addr = esphome::network::get_ip_address();

  /*
   * Send a message to the multicast group specified by the
   * groupSock sockaddr structure.
   */
  if (sd->bind((struct sockaddr *) &group_sock, sizeof(group_sock))) {
    ESP_LOGE(TAG, "Error binding to socket!");
    return;
  }
  uint8_t data[102];
  fill_buffer_(data);
  if (sd->write(data, sizeof(data)) < 0) {
    ESP_LOGE(TAG, "Error sending datagram message!");
  }
  sd->close();
}

void WakeOnLanButton::fill_buffer_(uint8_t *buff) {
  fill_preamble_(buff);
  fill_mac_address_(buff + 6);
  // fill_password(buff + 102)
}

void WakeOnLanButton::fill_preamble_(uint8_t *buff) { std::fill(buff, buff + 6, 255); }

void WakeOnLanButton::fill_mac_address_(uint8_t *buff) {
  for (int i = 0; i < 16; i++) {
    std::copy(std::begin(macaddr_), std::end(macaddr_), buff + (i * 6));
  }
}

}  // namespace wake_on_lan
}  // namespace esphome
