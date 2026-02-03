#include <cstring>
#include "hmac_sha256.h"
#if defined(USE_ESP32) || defined(USE_ESP8266) || defined(USE_RP2040) || defined(USE_LIBRETINY) || defined(USE_HOST)
#include "esphome/core/helpers.h"

namespace esphome::hmac_sha256 {

constexpr size_t SHA256_DIGEST_SIZE = 32;

#if defined(USE_ESP32) || defined(USE_LIBRETINY)

HmacSHA256::~HmacSHA256() { mbedtls_md_free(&this->ctx_); }

void HmacSHA256::init(const uint8_t *key, size_t len) {
  mbedtls_md_init(&this->ctx_);
  const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  mbedtls_md_setup(&this->ctx_, md_info, 1);  // 1 = HMAC mode
  mbedtls_md_hmac_starts(&this->ctx_, key, len);
}

void HmacSHA256::add(const uint8_t *data, size_t len) { mbedtls_md_hmac_update(&this->ctx_, data, len); }

void HmacSHA256::calculate() { mbedtls_md_hmac_finish(&this->ctx_, this->digest_); }

void HmacSHA256::get_bytes(uint8_t *output) { memcpy(output, this->digest_, SHA256_DIGEST_SIZE); }

void HmacSHA256::get_hex(char *output) {
  format_hex_to(output, SHA256_DIGEST_SIZE * 2 + 1, this->digest_, SHA256_DIGEST_SIZE);
}

bool HmacSHA256::equals_bytes(const uint8_t *expected) {
  return memcmp(this->digest_, expected, SHA256_DIGEST_SIZE) == 0;
}

bool HmacSHA256::equals_hex(const char *expected) {
  char hex_output[SHA256_DIGEST_SIZE * 2 + 1];
  this->get_hex(hex_output);
  hex_output[SHA256_DIGEST_SIZE * 2] = '\0';
  return strncmp(hex_output, expected, SHA256_DIGEST_SIZE * 2) == 0;
}

#else

HmacSHA256::~HmacSHA256() = default;

// HMAC block size for SHA256 (RFC 2104)
constexpr size_t HMAC_BLOCK_SIZE = 64;

void HmacSHA256::init(const uint8_t *key, size_t len) {
  uint8_t ipad[HMAC_BLOCK_SIZE], opad[HMAC_BLOCK_SIZE];

  memset(ipad, 0, sizeof(ipad));
  if (len > HMAC_BLOCK_SIZE) {
    sha256::SHA256 keysha256;
    keysha256.init();
    keysha256.add(key, len);
    keysha256.calculate();
    keysha256.get_bytes(ipad);
  } else {
    memcpy(ipad, key, len);
  }
  memcpy(opad, ipad, sizeof(opad));

  for (size_t i = 0; i < HMAC_BLOCK_SIZE; i++) {
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

#endif  // USE_ESP32 || USE_LIBRETINY

}  // namespace esphome::hmac_sha256
#endif
