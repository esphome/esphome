#pragma once
#ifdef USE_HOST
#include "ota_backend.h"

#include "esphome/components/md5/md5.h"

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
  OTAResponseTypes write(uint8_t *data, size_t len);
  OTAResponseTypes end();
  void abort();
  bool supports_compression() { return false; }

 protected:
  md5::MD5Digest md5_{};
  std::string staging_path_;
  std::string final_path_;
  size_t expected_size_{0};
  size_t bytes_written_{0};
  uint8_t expected_md5_[16]{};
  int fd_{-1};
  bool md5_set_{false};
};

std::unique_ptr<HostOTABackend> make_ota_backend();

}  // namespace esphome::ota
#endif
