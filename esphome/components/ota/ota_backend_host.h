#pragma once
#ifdef USE_HOST
#include "ota_backend.h"

namespace esphome::ota {

/// Stub OTA backend for host platform - allows compilation but does not implement OTA.
/// All operations return error codes immediately. This enables configurations with
/// OTA triggers to compile for host platform during development.
class HostOTABackend : public OTABackend {
 public:
  OTAResponseTypes begin(size_t image_size) override;
  void set_update_md5(const char *md5) override;
  OTAResponseTypes write(uint8_t *data, size_t len) override;
  OTAResponseTypes end() override;
  void abort() override;
  bool supports_compression() override { return false; }
};

}  // namespace esphome::ota
#endif
