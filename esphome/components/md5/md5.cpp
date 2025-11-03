#include <cstring>
#include "md5.h"
#ifdef USE_MD5
#include "esphome/core/helpers.h"

namespace esphome {
namespace md5 {

#if defined(USE_ARDUINO) && !defined(USE_RP2040) && !defined(USE_ESP32)
void MD5Digest::init() {
  memset(this->digest_, 0, 16);
  MD5Init(&this->ctx_);
}

void MD5Digest::add(const uint8_t *data, size_t len) { MD5Update(&this->ctx_, data, len); }

void MD5Digest::calculate() { MD5Final(this->digest_, &this->ctx_); }
#endif  // USE_ARDUINO && !USE_RP2040

#ifdef USE_ESP32
void MD5Digest::init() {
  memset(this->digest_, 0, 16);
  esp_rom_md5_init(&this->ctx_);
}

void MD5Digest::add(const uint8_t *data, size_t len) { esp_rom_md5_update(&this->ctx_, data, len); }

void MD5Digest::calculate() { esp_rom_md5_final(this->digest_, &this->ctx_); }
#endif  // USE_ESP32

#ifdef USE_RP2040
void MD5Digest::init() {
  memset(this->digest_, 0, 16);
  br_md5_init(&this->ctx_);
}

void MD5Digest::add(const uint8_t *data, size_t len) { br_md5_update(&this->ctx_, data, len); }

void MD5Digest::calculate() { br_md5_out(&this->ctx_, this->digest_); }
#endif  // USE_RP2040

}  // namespace md5
}  // namespace esphome
#endif
