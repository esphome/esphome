#include <cstdio>
#include <cstring>
#include "hmac_md5.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace hmac_md5 {
void HmacMD5::init(const uint8_t *key, size_t len) {
  uint8_t ipad[64], opad[64];

  memset(ipad, 0, sizeof(ipad));
  if (len > 64) {
    md5::MD5Digest keymd5;
    keymd5.init();
    keymd5.add(key, len);
    keymd5.calculate();
    keymd5.get_bytes(ipad);
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

void HmacMD5::add(const uint8_t *data, size_t len) { this->ihash_.add(data, len); }

void HmacMD5::calculate() {
  uint8_t ibytes[16];

  this->ihash_.calculate();
  this->ihash_.get_bytes(ibytes);

  this->ohash_.add(ibytes, sizeof(ibytes));
  this->ohash_.calculate();
}

void HmacMD5::get_bytes(uint8_t *output) { this->ohash_.get_bytes(output); }

void HmacMD5::get_hex(char *output) { this->ohash_.get_hex(output); }

bool HmacMD5::equals_bytes(const uint8_t *expected) { return this->ohash_.equals_bytes(expected); }

bool HmacMD5::equals_hex(const char *expected) { return this->ohash_.equals_hex(expected); }

}  // namespace hmac_md5
}  // namespace esphome
