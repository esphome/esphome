#include "sha256.h"

// Only compile SHA256 implementation on platforms that support it
#if defined(USE_ESP32) || defined(USE_ESP8266) || defined(USE_RP2040) || defined(USE_LIBRETINY) || defined(USE_HOST)

#include "esphome/core/helpers.h"
#include <cstring>

namespace esphome::sha256 {

#if defined(USE_ESP32) || defined(USE_LIBRETINY)

// CRITICAL ESP32-S3 HARDWARE SHA ACCELERATION REQUIREMENTS:
//
// The ESP32-S3 uses hardware DMA for SHA acceleration. The mbedtls_sha256_context structure contains
// internal state that the DMA engine references. This imposes two critical constraints:
//
// 1. NO VARIABLE LENGTH ARRAYS (VLAs): VLAs corrupt the stack layout, causing the DMA engine to
//    write to incorrect memory locations. This results in null pointer dereferences and crashes.
//    ALWAYS use fixed-size arrays (e.g., char buf[65], not char buf[size+1]).
//
// 2. SAME STACK FRAME ONLY: The SHA256 object must be created and used entirely within the same
//    function. NEVER pass the SHA256 object or HashBase pointer to another function. When the stack
//    frame changes (function call/return), the DMA references become invalid and will produce
//    truncated hash output (20 bytes instead of 32) or corrupt memory.
//
// CORRECT USAGE:
//   void my_function() {
//     sha256::SHA256 hasher;  // Created locally
//     hasher.init();
//     hasher.add(data, len);  // Any size, no chunking needed
//     hasher.calculate();
//     bool ok = hasher.equals_hex(expected);
//     // hasher destroyed when function returns
//   }
//
// INCORRECT USAGE (WILL FAIL ON ESP32-S3):
//   void my_function() {
//     sha256::SHA256 hasher;
//     helper(&hasher);  // WRONG: Passed to different stack frame
//   }
//   void helper(HashBase *h) {
//     h->init();  // WRONG: Will produce truncated/corrupted output
//   }

SHA256::~SHA256() { mbedtls_sha256_free(&this->ctx_); }

void SHA256::init() {
  mbedtls_sha256_init(&this->ctx_);
  mbedtls_sha256_starts(&this->ctx_, 0);  // 0 = SHA256, not SHA224
}

void SHA256::add(const uint8_t *data, size_t len) { mbedtls_sha256_update(&this->ctx_, data, len); }

void SHA256::calculate() { mbedtls_sha256_finish(&this->ctx_, this->digest_); }

#elif defined(USE_ESP8266) || defined(USE_RP2040)

SHA256::~SHA256() = default;

void SHA256::init() {
  br_sha256_init(&this->ctx_);
  this->calculated_ = false;
}

void SHA256::add(const uint8_t *data, size_t len) { br_sha256_update(&this->ctx_, data, len); }

void SHA256::calculate() {
  if (!this->calculated_) {
    br_sha256_out(&this->ctx_, this->digest_);
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
    EVP_DigestFinal_ex(this->ctx_, this->digest_, &len);
    this->calculated_ = true;
  }
}

#else
#error "SHA256 not supported on this platform"
#endif

}  // namespace esphome::sha256

#endif  // Platform check
