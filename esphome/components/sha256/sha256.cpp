#include "sha256.h"

// Only compile SHA256 implementation on platforms that support it
#if defined(USE_ESP32) || defined(USE_ESP8266) || defined(USE_RP2040) || defined(USE_LIBRETINY) || defined(USE_HOST)

#include "esphome/core/helpers.h"
#include <cstring>

namespace esphome::sha256 {

#if defined(USE_ESP32) || defined(USE_LIBRETINY)

SHA256::~SHA256() { mbedtls_sha256_free(&this->ctx_); }

void SHA256::init() {
  mbedtls_sha256_init(&this->ctx_);
  mbedtls_sha256_starts(&this->ctx_, 0);  // 0 = SHA256, not SHA224
}

void SHA256::add(const uint8_t *data, size_t len) { mbedtls_sha256_update(&this->ctx_, data, len); }

void SHA256::calculate() { mbedtls_sha256_finish(&this->ctx_, this->hash_); }

#elif defined(USE_ESP8266) || defined(USE_RP2040)

SHA256::~SHA256() = default;

void SHA256::init() {
  br_sha256_init(&this->ctx_);
  this->calculated_ = false;
}

void SHA256::add(const uint8_t *data, size_t len) { br_sha256_update(&this->ctx_, data, len); }

void SHA256::calculate() {
  if (!this->calculated_) {
    br_sha256_out(&this->ctx_, this->hash_);
    this->calculated_ = true;
  }
}

#elif defined(USE_HOST)

SHA256::~SHA256() {
  if (this->ctx_) {
    EVP_MD_CTX_free(this->ctx_);
  }
}

void SHA256::init() {
  if (this->ctx_) {
    EVP_MD_CTX_free(this->ctx_);
  }
  this->ctx_ = EVP_MD_CTX_new();
  EVP_DigestInit_ex(this->ctx_, EVP_sha256(), nullptr);
  this->calculated_ = false;
}

void SHA256::add(const uint8_t *data, size_t len) {
  if (!this->ctx_) {
    this->init();
  }
  EVP_DigestUpdate(this->ctx_, data, len);
}

void SHA256::calculate() {
  if (!this->ctx_) {
    this->init();
  }
  if (!this->calculated_) {
    unsigned int len = 32;
    EVP_DigestFinal_ex(this->ctx_, this->hash_, &len);
    this->calculated_ = true;
  }
}

#else
#error "SHA256 not supported on this platform"
#endif

void SHA256::get_bytes(uint8_t *output) { memcpy(output, this->hash_, 32); }

void SHA256::get_hex(char *output) {
  for (size_t i = 0; i < 32; i++) {
    uint8_t byte = this->hash_[i];
    output[i * 2] = format_hex_char(byte >> 4);
    output[i * 2 + 1] = format_hex_char(byte & 0x0F);
  }
}

std::string SHA256::get_hex_string() {
  char buf[65];
  this->get_hex(buf);
  return std::string(buf);
}

bool SHA256::equals_bytes(const uint8_t *expected) { return memcmp(this->hash_, expected, 32) == 0; }

bool SHA256::equals_hex(const char *expected) {
  uint8_t parsed[32];
  if (!parse_hex(expected, parsed, 32)) {
    return false;
  }
  return this->equals_bytes(parsed);
}

}  // namespace esphome::sha256

#endif  // Platform check
