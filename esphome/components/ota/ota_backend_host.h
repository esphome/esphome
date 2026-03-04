#pragma once
#ifdef USE_HOST
#include "ota_backend.h"

namespace esphome::ota {

/// Stub OTA backend for host platform - allows compilation but does not implement OTA.
/// All operations return error codes immediately. This enables configurations with
/// OTA triggers to compile for host platform during development.
class HostOTABackend final {
 public:
  OTAResponseTypes begin(size_t image_size);
  void set_update_md5(const char *md5);
  OTAResponseTypes write(uint8_t *data, size_t len);
  OTAResponseTypes end();
  void abort();
  bool supports_compression() { return false; }
};

std::unique_ptr<HostOTABackend> make_ota_backend();

}  // namespace esphome::ota
#endif
