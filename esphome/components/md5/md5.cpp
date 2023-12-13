#include <cstdio>
#include <cstring>
#include "md5.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace md5 {

#if defined(USE_ARDUINO) && !defined(USE_RP2040)
void MD5Digest::init() {
  memset(this->digest_, 0, 16);
  MD5Init(&this->ctx_);
}

void MD5Digest::add(const uint8_t *data, size_t len) { MD5Update(&this->ctx_, data, len); }

void MD5Digest::calculate() { MD5Final(this->digest_, &this->ctx_); }
#endif  // USE_ARDUINO && !USE_RP2040

#ifdef USE_ESP_IDF
void MD5Digest::init() {
  memset(this->digest_, 0, 16);
  esp_rom_md5_init(&this->ctx_);
}

void MD5Digest::add(const uint8_t *data, size_t len) { esp_rom_md5_update(&this->ctx_, data, len); }

void MD5Digest::calculate() { esp_rom_md5_final(this->digest_, &this->ctx_); }
#endif  // USE_ESP_IDF

#ifdef USE_RP2040
void MD5Digest::init() {
  memset(this->digest_, 0, 16);
  br_md5_init(&this->ctx_);
}

void MD5Digest::add(const uint8_t *data, size_t len) { br_md5_update(&this->ctx_, data, len); }

void MD5Digest::calculate() { br_md5_out(&this->ctx_, this->digest_); }
#endif  // USE_RP2040

void MD5Digest::get_bytes(uint8_t *output) { memcpy(output, this->digest_, 16); }

void MD5Digest::get_hex(char *output) {
  for (size_t i = 0; i < 16; i++) {
    sprintf(output + i * 2, "%02x", this->digest_[i]);
  }
}

bool MD5Digest::equals_bytes(const uint8_t *expected) {
  for (size_t i = 0; i < 16; i++) {
    if (expected[i] != this->digest_[i]) {
      return false;
    }
  }
  return true;
}

bool MD5Digest::equals_hex(const char *expected) {
  uint8_t parsed[16];
  if (!parse_hex(expected, parsed, 16))
    return false;
  return equals_bytes(parsed);
}

}  // namespace md5
}  // namespace esphome
