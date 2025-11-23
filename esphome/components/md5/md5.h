#pragma once

#include "esphome/core/defines.h"
#ifdef USE_MD5

#include "esphome/core/hash_base.h"

#ifdef USE_ESP32
#include "esp_rom_md5.h"
#define MD5_CTX_TYPE md5_context_t
#endif

#if defined(USE_ARDUINO) && defined(USE_ESP8266)
#include <md5.h>
#define MD5_CTX_TYPE md5_context_t
#endif

#ifdef USE_RP2040
#include <MD5Builder.h>
#define MD5_CTX_TYPE br_md5_context
#endif

#if defined(USE_LIBRETINY)
#include <MD5.h>
#define MD5_CTX_TYPE LT_MD5_CTX_T
#endif

namespace esphome {
namespace md5 {

class MD5Digest : public HashBase {
 public:
  MD5Digest() = default;
  ~MD5Digest() override = default;

  /// Initialize a new MD5 digest computation.
  void init() override;

  /// Add bytes of data for the digest.
  void add(const uint8_t *data, size_t len) override;
  using HashBase::add;  // Bring base class overload into scope

  /// Compute the digest, based on the provided data.
  void calculate() override;

  /// Get the size of the hash in bytes (16 for MD5)
  size_t get_size() const override { return 16; }

 protected:
  MD5_CTX_TYPE ctx_{};
};

}  // namespace md5
}  // namespace esphome
#endif
