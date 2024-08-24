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
#define TRANSPORT_HEADER_SIZE 3

static const uint8_t transport_header[TRANSPORT_HEADER_SIZE] = {0xC1, 0x99, 0x83};

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

  inline void info(std::string place) {
    ESP_LOGVV("Packet", "%s: M:%s A:0x%06x R:0x%02x C:0x%04x S:%02x", place.c_str(), this->to_str().c_str(),
              this->app_id, this->ref_id, this->random, this->size);
  }

  inline ESPNowPacket(const uint64_t mac64_, const uint8_t *data_, uint8_t size_,
                      uint32_t app_id_) ESPHOME_ALWAYS_INLINE : mac64(mac64_),
                                                                size(size_),
                                                                app_id(app_id_),
                                                                retrys(0) {
    if (mac64_ == 0)
      this->mac64 = ESPNOW_BROADCAST_ADDR;
    this->is_broadcast = this->mac64 == ESPNOW_BROADCAST_ADDR;

    this->ref_id = 0;

    this->size = std::min(MAX_ESPNOW_DATA_SIZE, size_);
    std::memcpy(&data, (uint8_t *) data_, this->size);

    this->data[this->size + 1] = 0;
    this->recalc();
    this->info("create");
  }

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

  bool is_valid() {
    uint16_t crc = random;
    recalc();
    bool valid = (std::memcmp(&header, &transport_header, 3) == 0);
    valid &= (this->app_id != 0);
    valid &= (this->random == crc);
    if (!valid) {
      ESP_LOGV("Packet", "Invalid H:%02x%02x%02x A:%06x R:%02x C:%04x ipv. %04x, %d&%d&%d=%d\n", this->header[0],
               this->header[1], this->header[2], this->app_id, this->ref_id, crc, this->random,
               std::memcmp(&header, &transport_header, 3) == 0, (this->app_id != 0), (this->random == crc), valid);
    }

    this->random = crc;
    return valid;
  }

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
