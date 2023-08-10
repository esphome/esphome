
#ifdef USE_ESP32
#include "utils.h"

#ifndef SCNx8
#define SCNx8 "hhx"
#endif

#define STRINGIZE(x) #x
#define TABLE_ENTRY(x) x, #x

namespace esphome {
namespace esp32_bt_classic {

typedef struct {
  esp_bt_status_t code;
  const char *msg;
} esp_bt_status_msg_t;

static const esp_bt_status_msg_t esp_bt_status_msg_table[] = {
    TABLE_ENTRY(ESP_BT_STATUS_SUCCESS),
    TABLE_ENTRY(ESP_BT_STATUS_FAIL),
    TABLE_ENTRY(ESP_BT_STATUS_NOT_READY),
    TABLE_ENTRY(ESP_BT_STATUS_NOMEM),
    TABLE_ENTRY(ESP_BT_STATUS_BUSY),
    TABLE_ENTRY(ESP_BT_STATUS_DONE),
    TABLE_ENTRY(ESP_BT_STATUS_UNSUPPORTED),
    TABLE_ENTRY(ESP_BT_STATUS_PARM_INVALID),
    TABLE_ENTRY(ESP_BT_STATUS_UNHANDLED),
    TABLE_ENTRY(ESP_BT_STATUS_AUTH_FAILURE),
    TABLE_ENTRY(ESP_BT_STATUS_RMT_DEV_DOWN),
    TABLE_ENTRY(ESP_BT_STATUS_AUTH_REJECTED),
    TABLE_ENTRY(ESP_BT_STATUS_INVALID_STATIC_RAND_ADDR),
    TABLE_ENTRY(ESP_BT_STATUS_PENDING),
    TABLE_ENTRY(ESP_BT_STATUS_UNACCEPT_CONN_INTERVAL),
    TABLE_ENTRY(ESP_BT_STATUS_PARAM_OUT_OF_RANGE),
    TABLE_ENTRY(ESP_BT_STATUS_TIMEOUT),
    TABLE_ENTRY(ESP_BT_STATUS_PEER_LE_DATA_LEN_UNSUPPORTED),
    TABLE_ENTRY(ESP_BT_STATUS_CONTROL_LE_DATA_LEN_UNSUPPORTED),
    TABLE_ENTRY(ESP_BT_STATUS_ERR_ILLEGAL_PARAMETER_FMT),
    TABLE_ENTRY(ESP_BT_STATUS_MEMORY_FULL),
    TABLE_ENTRY(ESP_BT_STATUS_EIR_TOO_LARGE)};

const char *esp_bt_status_to_str(esp_bt_status_t code) {
  for (int i = 0; i < sizeof(esp_bt_status_msg_table) / sizeof(esp_bt_status_msg_table[0]); ++i) {
    if (esp_bt_status_msg_table[i].code == code) {
      return esp_bt_status_msg_table[i].msg;
    }
  }

  return "Unknown Status";
}

void uint64_to_bd_addr(uint64_t address, esp_bd_addr_t &bd_addr) {
  bd_addr[0] = (address >> 40) & 0xff;
  bd_addr[1] = (address >> 32) & 0xff;
  bd_addr[2] = (address >> 24) & 0xff;
  bd_addr[3] = (address >> 16) & 0xff;
  bd_addr[4] = (address >> 8) & 0xff;
  bd_addr[5] = (address >> 0) & 0xff;
}

uint64_t bd_addr_to_uint64(const esp_bd_addr_t address) {
  uint64_t u = 0;
  u |= uint64_t(address[0] & 0xFF) << 40;
  u |= uint64_t(address[1] & 0xFF) << 32;
  u |= uint64_t(address[2] & 0xFF) << 24;
  u |= uint64_t(address[3] & 0xFF) << 16;
  u |= uint64_t(address[4] & 0xFF) << 8;
  u |= uint64_t(address[5] & 0xFF) << 0;
  return u;
}

std::string u64_addr_to_str(uint64_t address) {
  esp_bd_addr_t addr;
  char mac[24];
  uint64_to_bd_addr(address, addr);
  snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", EXPAND_MAC_F(addr));
  return mac;
}

std::string bd_addr_to_str(const esp_bd_addr_t &addr) {
  char mac[24];
  snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", EXPAND_MAC_F(addr));
  return mac;
}

bool str_to_bd_addr(const char *addr_str, esp_bd_addr_t &addr) {
  if (strlen(addr_str) != 17) {
    ESP_LOGE(TAG, "Invalid string length for MAC address. Got '%s'", addr_str);
    return false;
  }

  uint8_t *p = addr;
  // Scan for MAC with semicolon separators
  int args_found = sscanf(addr_str, "%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8, &p[0], &p[1],
                          &p[2], &p[3], &p[4], &p[5]);
  if (args_found != 6) {
    ESP_LOGE(TAG, "Invalid MAC address: '%s'", addr_str);
    return false;
  }

  ESP_LOGVV(TAG, "Created mac_addr from string : %02X:%02X:%02X:%02X:%02X:%02X", EXPAND_MAC_F(addr));
  return true;
}

}  // namespace esp32_bt_classic
}  // namespace esphome

#endif  // USE_ESP32
