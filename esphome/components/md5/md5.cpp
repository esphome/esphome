#include <cstring>
#include "md5.h"
#ifdef USE_MD5
#include "esphome/core/helpers.h"

namespace esphome::md5 {

#if defined(USE_ARDUINO) && !defined(USE_RP2) && !defined(USE_ESP32)
void MD5Digest::init() {
  memset(this->digest_, 0, 16);
  MD5Init(&this->ctx_);
}

void MD5Digest::add(const uint8_t *data, size_t len) { MD5Update(&this->ctx_, data, len); }

void MD5Digest::calculate() { MD5Final(this->digest_, &this->ctx_); }
#endif  // USE_ARDUINO && !USE_RP2

#ifdef USE_ESP32
void MD5Digest::init() {
  memset(this->digest_, 0, 16);
  esp_rom_md5_init(&this->ctx_);
}

void MD5Digest::add(const uint8_t *data, size_t len) { esp_rom_md5_update(&this->ctx_, data, len); }

void MD5Digest::calculate() { esp_rom_md5_final(this->digest_, &this->ctx_); }
#endif  // USE_ESP32

#ifdef USE_RP2
void MD5Digest::init() {
  memset(this->digest_, 0, 16);
  br_md5_init(&this->ctx_);
}

void MD5Digest::add(const uint8_t *data, size_t len) { br_md5_update(&this->ctx_, data, len); }

void MD5Digest::calculate() { br_md5_out(&this->ctx_, this->digest_); }
#endif  // USE_RP2

#if defined(USE_ZEPHYR) && !defined(USE_NRF52)
#if MBEDTLS_VERSION_MAJOR >= 4
MD5Digest::~MD5Digest() { psa_hash_abort(&this->ctx_); }

void MD5Digest::init() {
  memset(this->digest_, 0, 16);
  psa_crypto_init();
  this->ctx_ = PSA_HASH_OPERATION_INIT;
  psa_hash_setup(&this->ctx_, PSA_ALG_MD5);
}

void MD5Digest::add(const uint8_t *data, size_t len) { psa_hash_update(&this->ctx_, data, len); }

void MD5Digest::calculate() {
  size_t out_len;
  psa_hash_finish(&this->ctx_, this->digest_, 16, &out_len);
}
#else
MD5Digest::~MD5Digest() { mbedtls_md5_free(&this->ctx_); }

void MD5Digest::init() {
  memset(this->digest_, 0, 16);
  mbedtls_md5_init(&this->ctx_);
  mbedtls_md5_starts(&this->ctx_);
}

void MD5Digest::add(const uint8_t *data, size_t len) { mbedtls_md5_update(&this->ctx_, data, len); }

void MD5Digest::calculate() { mbedtls_md5_finish(&this->ctx_, this->digest_); }
#endif
#elif defined(USE_HOST)
MD5Digest::~MD5Digest() {
  if (this->ctx_) {
    EVP_MD_CTX_free(this->ctx_);
  }
}

void MD5Digest::init() {
  if (this->ctx_) {
    EVP_MD_CTX_free(this->ctx_);
  }
  this->ctx_ = EVP_MD_CTX_new();
  EVP_DigestInit_ex(this->ctx_, EVP_md5(), nullptr);
  this->calculated_ = false;
  memset(this->digest_, 0, 16);
}

void MD5Digest::add(const uint8_t *data, size_t len) {
  if (!this->ctx_) {
    this->init();
  }
  EVP_DigestUpdate(this->ctx_, data, len);
}

void MD5Digest::calculate() {
  if (!this->ctx_) {
    this->init();
  }
  if (!this->calculated_) {
    unsigned int len = 16;
    EVP_DigestFinal_ex(this->ctx_, this->digest_, &len);
    this->calculated_ = true;
  }
}
#else
MD5Digest::~MD5Digest() = default;
#endif

}  // namespace esphome::md5

#endif
