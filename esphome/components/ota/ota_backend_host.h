#pragma once
#ifdef USE_HOST
#include "ota_backend.h"

#include "esphome/components/md5/md5.h"
#include "esphome/core/defines.h"

#ifdef USE_SHA256
#include "esphome/components/sha256/sha256.h"
#endif

#include <cstddef>
#include <cstdint>
#include <string>

namespace esphome::ota {

/// Host OTA backend: stages new binary to `<exe>.ota.new`, validates ELF/Mach-O
/// matches the running arch, renames over `<exe>`, and arms execv via arch_restart().
class HostOTABackend final {
 public:
  OTAResponseTypes begin(size_t image_size, OTAType ota_type = OTA_TYPE_UPDATE_APP);
  void set_update_md5(const char *md5);
  void set_update_sha256(const char *sha256);
  OTAResponseTypes write(uint8_t *data, size_t len);
  OTAResponseTypes end();
  void abort();
  static constexpr bool supports_compression() { return false; }
#ifdef USE_SHA256
  static constexpr bool supports_sha256_checksum() { return true; }
#else
  static constexpr bool supports_sha256_checksum() { return false; }
#endif
  static constexpr bool requires_sha256_checksum() { return false; }

 protected:
#ifdef USE_SHA256
  void checksum_add_(const uint8_t *data, size_t len) {
    if (this->sha256_set_) {
      this->sha256_.add(data, len);
    } else {
      this->md5_.add(data, len);
    }
  }
#else
  void checksum_add_(const uint8_t *data, size_t len) { this->md5_.add(data, len); }
#endif
  UpdateLock lock_;
  md5::MD5Digest md5_{};
  std::string staging_path_;
  std::string final_path_;
  size_t expected_size_{0};
  size_t bytes_written_{0};
  uint8_t expected_md5_[16]{};
  int fd_{-1};
  bool md5_set_{false};
#ifdef USE_SHA256
  sha256::SHA256 sha256_{};
  uint8_t expected_sha256_[32];
  bool sha256_set_{false};
#endif
};

std::unique_ptr<HostOTABackend> make_ota_backend();

}  // namespace esphome::ota
#endif
