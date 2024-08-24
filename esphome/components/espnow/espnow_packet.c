#include "espnow_packet.h"
#if defined(USE_ESP32)

#include "esphome/core/log.h"

static const char *const TAG = "espnow_packet";

ESPNowPacket::ESPNowPacket(const uint64_t mac64, const uint8_t *data, uint8_t size,
                           uint32_t app_id) ESPHOME_ALWAYS_INLINE : mac64(mac64),
                                                                    size(size),
                                                                    app_id(app_id),
                                                                    retrys(0) {
  if (this->mac64 == 0)
    this->mac64 = ESPNOW_BROADCAST_ADDR;
  this->is_broadcast = this->mac64 == ESPNOW_BROADCAST_ADDR;

  this->ref_id = 0;

  this->size = std::min(MAX_ESPNOW_DATA_SIZE, size);
  std::memcpy(&this->data, (uint8_t *) data, this->size);

  this->data[this->size + 1] = 0;
  this->recalc();
  this->info("create");
}

inline void ESPNowPacket::info(std::string place) {
  ESP_LOGVV(TAG, "%s: M:%s A:0x%06x R:0x%02x C:0x%04x S:%02x", place.c_str(), this->to_str().c_str(), this->app_id,
            this->ref_id, this->random, this->size);
}

bool ESPNowPacket::is_valid() {
  uint16_t crc = this->crc16;
  recalc();
  bool valid = (std::memcmp(&header, &transport_header, 3) == 0);
  valid &= (this->app_id != 0);
  valid &= (this->crc16 == crc);
  if (!valid) {
    ESP_LOGV("Packet", "Invalid H:%02x%02x%02x A:%06x R:%02x C:%04x ipv. %04x, %d&%d&%d=%d\n", this->header[0],
             this->header[1], this->header[2], this->app_id, this->ref_id, crc, this->crc16,
             std::memcmp(&header, &transport_header, 3) == 0, (this->app_id != 0), (this->crc16 == crc), valid);
  }

  this->crc16 = crc;
  return valid;
}

#endif
