#pragma once

// #if defined(USE_ESP32)

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <esp_now.h>
#include "esp_crc.h"

#include <array>
#include <memory>
#include <queue>
#include <vector>

namespace esphome {
namespace espnow {

typedef uint8_t espnow_addr_t[6];

static const uint64_t ESPNOW_BROADCAST_ADDR = 0xFFFFFFFFFFFF;
static espnow_addr_t ESPNOW_ADDR_SELF = {0};
static const uint8_t MAX_ESPNOW_DATA_SIZE = 240;

static const uint32_t transport_header = 0xC19983;

template<typename... Args> std::string string_format(const std::string &format, Args... args) {
  int size_s = std::snprintf(nullptr, 0, format.c_str(), args...) + 1;  // Extra space for '\0'
  if (size_s <= 0) {
    return ("Error during formatting.");
  }
  auto size = static_cast<size_t>(size_s);
  std::unique_ptr<char[]> buf(new char[size]);
  std::snprintf(buf.get(), size, format.c_str(), args...);
  return std::string(buf.get(), buf.get() + size - 1);  // We don't want the '\0' inside
}

static uint8_t last_ref_id = 0;

struct ESPNowPacket {
  uint64_t mac64 = 0;
  uint8_t size = 0;
  uint8_t rssi = 0;
  uint8_t retrys : 4;
  uint8_t is_broadcast : 1;
  uint8_t dummy : 3;
  uint32_t timestamp = 0;

  union {
    uint8_t content[MAX_ESPNOW_DATA_SIZE + 11];
    struct {
      uint8_t header[3] = {0xC1, 0x99, 0x83};
      uint32_t app_id = 0xFFFFFF;
      uint1_t ref_id = 0x99;
      uint8_t crc16 = 0x1234;
      uint8_t data[MAX_ESPNOW_DATA_SIZE];
      uint8_t space = 0;
    } __attribute__((packed));
  };

  inline ESPNowPacket() ESPHOME_ALWAYS_INLINE : retrys(0) {}
  inline ESPNowPacket(const uint64_t mac64, const uint8_t *data, uint8_t size, uint32_t app_id);

  inline void info(std::string place);

  inline void get_mac(espnow_addr_t *mac_addres) { std::memcpy(mac_addres, &mac64, 6); }
  inline void set_mac(espnow_addr_t *mac_addres) { this->mac64 = this->to_mac64(mac_addres); }

  uint64_t to_mac64(espnow_addr_t *mac_addres) {
    uint64_t result;
    std::memcpy(&result, mac_addres, 6);
    return result;
  }

  void retry() {
    if (this->retrys < 7) {
      retrys = retrys + 1;
    }
  }

  inline void recalc() {
    random = 0;
    random = esp_crc16_le(ref_id, (uint8_t *) &content, 10 + size);
  }

  bool is_valid();

  inline std::string to_str(uint64_t mac64_ = 0) {
    espnow_addr_t mac;
    if (mac64_ == 0)
      mac64_ = this->mac64;
    memcpy((void *) &mac, &mac64_, 6);
    return string_format("{\"%02x:%02x:%02x:%02x:%02x:%02x\"}", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  }

  inline uint8_t *dataptr() { return (uint8_t *) &content; }
};

}  // namespace espnow
}  // namespace esphome
