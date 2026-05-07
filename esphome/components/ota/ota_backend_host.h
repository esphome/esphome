#pragma once
#ifdef USE_HOST
#include "ota_backend.h"

#include "esphome/components/md5/md5.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace esphome::ota {

/// Host platform OTA backend.
///
/// Receives the new binary into a sibling temp file (`<exe>.ota.new`),
/// verifies size, MD5, and that the payload is an executable matching the
/// running architecture (ELF on Linux, Mach-O on macOS), atomically renames
/// it over the running executable, and arms a pending re-exec. The actual
/// execv is performed by `arch_restart()` after `App::safe_reboot()` runs
/// shutdown hooks -- same lifecycle as a real device reboot.
class HostOTABackend final {
 public:
  OTAResponseTypes begin(size_t image_size, OTAType ota_type = OTA_TYPE_UPDATE_APP);
  void set_update_md5(const char *md5);
  OTAResponseTypes write(uint8_t *data, size_t len);
  OTAResponseTypes end();
  void abort();
  bool supports_compression() { return false; }

 protected:
  int fd_{-1};
  std::string staging_path_;
  std::string final_path_;
  size_t expected_size_{0};
  size_t bytes_written_{0};
  uint8_t expected_md5_[16]{};
  bool md5_set_{false};
  md5::MD5Digest md5_{};
};

std::unique_ptr<HostOTABackend> make_ota_backend();

}  // namespace esphome::ota
#endif
