#include "sha256.h"
#include "esphome/core/helpers.h"
#include <cstring>

#ifdef USE_ESP32
#include "mbedtls/sha256.h"
#elif defined(USE_ESP8266) || defined(USE_RP2040)
#include <SHA256.h>
#endif

namespace esphome {
namespace sha256 {

#ifdef USE_ESP32
struct SHA256::SHA256Context {
  mbedtls_sha256_context ctx;
  uint8_t hash[32];
};

SHA256::~SHA256() {
  if (this->ctx_) {
    mbedtls_sha256_free(&this->ctx_->ctx);
  }
}

void SHA256::init() {
  if (!this->ctx_) {
    this->ctx_ = std::make_unique<SHA256Context>();
  }
  mbedtls_sha256_init(&this->ctx_->ctx);
  mbedtls_sha256_starts(&this->ctx_->ctx, 0);  // 0 = SHA256, not SHA224
}

void SHA256::add(const uint8_t *data, size_t len) {
  if (!this->ctx_) {
    this->init();
  }
  mbedtls_sha256_update(&this->ctx_->ctx, data, len);
}

void SHA256::calculate() {
  if (!this->ctx_) {
    this->init();
  }
  mbedtls_sha256_finish(&this->ctx_->ctx, this->ctx_->hash);
}

#elif defined(USE_ESP8266) || defined(USE_RP2040)

struct SHA256::SHA256Context {
  ::SHA256 sha;
  uint8_t hash[32];
  bool calculated{false};
};

SHA256::~SHA256() = default;

void SHA256::init() {
  if (!this->ctx_) {
    this->ctx_ = std::make_unique<SHA256Context>();
  }
  this->ctx_->sha.reset();
  this->ctx_->calculated = false;
}

void SHA256::add(const uint8_t *data, size_t len) {
  if (!this->ctx_) {
    this->init();
  }
  this->ctx_->sha.update(data, len);
}

void SHA256::calculate() {
  if (!this->ctx_) {
    this->init();
  }
  if (!this->ctx_->calculated) {
    this->ctx_->sha.finalize(this->ctx_->hash, 32);
    this->ctx_->calculated = true;
  }
}

#else
#error "SHA256 not supported on this platform"
#endif

void SHA256::get_bytes(uint8_t *output) {
  if (!this->ctx_) {
    memset(output, 0, 32);
    return;
  }
  memcpy(output, this->ctx_->hash, 32);
}

void SHA256::get_hex(char *output) {
  if (!this->ctx_) {
    memset(output, '0', 64);
    output[64] = '\0';
    return;
  }
  for (size_t i = 0; i < 32; i++) {
    sprintf(output + i * 2, "%02x", this->ctx_->hash[i]);
  }
}

std::string SHA256::get_hex_string() {
  char buf[65];
  this->get_hex(buf);
  return std::string(buf);
}

bool SHA256::equals_bytes(const uint8_t *expected) {
  if (!this->ctx_) {
    return false;
  }
  return memcmp(this->ctx_->hash, expected, 32) == 0;
}

bool SHA256::equals_hex(const char *expected) {
  if (!this->ctx_) {
    return false;
  }
  uint8_t parsed[32];
  if (!parse_hex(expected, parsed, 32)) {
    return false;
  }
  return this->equals_bytes(parsed);
}

}  // namespace sha256
}  // namespace esphome
