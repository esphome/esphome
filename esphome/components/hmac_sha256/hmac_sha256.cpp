#include <cstdio>
#include <cstring>
#include "hmac_sha256.h"
#if defined(USE_ESP32) || defined(USE_ESP8266) || defined(USE_RP2040) || defined(USE_LIBRETINY) || defined(USE_HOST)
#include "esphome/core/helpers.h"

namespace esphome::hmac_sha256 {
void HmacSHA256::init(const uint8_t *key, size_t len) {
  uint8_t ipad[64], opad[64];

  memset(ipad, 0, sizeof(ipad));
  if (len > 64) {
    sha256::SHA256 keysha256;
    keysha256.init();
    keysha256.add(key, len);
    keysha256.calculate();
    keysha256.get_bytes(ipad);
  } else {
    memcpy(ipad, key, len);
  }
  memcpy(opad, ipad, sizeof(opad));

  for (int i = 0; i < 64; i++) {
    ipad[i] ^= 0x36;
    opad[i] ^= 0x5c;
  }

  this->ihash_.init();
  this->ihash_.add(ipad, sizeof(ipad));

  this->ohash_.init();
  this->ohash_.add(opad, sizeof(opad));
}

void HmacSHA256::add(const uint8_t *data, size_t len) { this->ihash_.add(data, len); }

void HmacSHA256::calculate() {
  uint8_t ibytes[32];

  this->ihash_.calculate();
  this->ihash_.get_bytes(ibytes);

  this->ohash_.add(ibytes, sizeof(ibytes));
  this->ohash_.calculate();
}

void HmacSHA256::get_bytes(uint8_t *output) { this->ohash_.get_bytes(output); }

void HmacSHA256::get_hex(char *output) { this->ohash_.get_hex(output); }

bool HmacSHA256::equals_bytes(const uint8_t *expected) { return this->ohash_.equals_bytes(expected); }

bool HmacSHA256::equals_hex(const char *expected) { return this->ohash_.equals_hex(expected); }

}  // namespace esphome::hmac_sha256
#endif
