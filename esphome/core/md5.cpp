#include <cstdio>
#include <cstring>
#include "md5.h"
#include "esp32/rom/md5_hash.h"

namespace esphome {

uint8_t hex2byte(char hex) {
  if (hex >= '0' && hex <= '9') { return hex - '0'; }
  if (hex >= 'a' && hex <= 'f') { return hex - 'a' + 10; }
  if (hex >= 'A' && hex <= 'F') { return hex - 'A' + 10; }
  return 0;
}

void MD5Digest::init() {
  memset(this->digest_, 0, 16);
  MD5Init(&this->ctx_);
}

void MD5Digest::add(uint8_t *data, size_t len) {
  MD5Update(&this->ctx_, data, len);
}

void MD5Digest::calculate() {
  MD5Final(this->digest_, &this->ctx_);
}

void MD5Digest::get_bytes(uint8_t *output) {
  memcpy(output, this->digest_, 16);
}

void MD5Digest::get_hex(char *output) {
  for (size_t i = 0; i < 16; i++) {
    sprintf(output + i*2, "%02x", this->digest_[i]);
  }
}

bool MD5Digest::equals_bytes(const char *expected) {
  for (size_t i = 0; i < 16; i++) {
    if (expected[i] != this->digest_[i]) {
      return false;
    }
  }
  return true;
}

bool MD5Digest::equals_hex(const char *expected) {
  for (size_t i = 0; i < 16; i++) {
    auto value = (hex2byte(expected[i*2]) << 4) | hex2byte(expected[i*2+1]);
    if (value != this->digest_[i]) {
      return false;
    }
  }
  return true;
}

}  // namespace esphome
