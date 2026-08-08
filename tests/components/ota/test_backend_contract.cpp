// Pins the OTA backend contract concept so the surface it enforces cannot
// drift unnoticed: a minimal conforming type must satisfy it, and a type
// missing a method or returning the wrong type must not.

#include "esphome/components/ota/ota_backend.h"

#include <gtest/gtest.h>

namespace esphome::ota::testing {

struct MinimalBackend {
  OTAResponseTypes begin(size_t image_size, OTAType ota_type = OTA_TYPE_UPDATE_APP) { return OTA_RESPONSE_OK; }
  void set_update_md5(const char *md5) {}
  OTAResponseTypes write(uint8_t *data, size_t len) { return OTA_RESPONSE_OK; }
  OTAResponseTypes end() { return OTA_RESPONSE_OK; }
  void abort() {}
  bool supports_compression() { return false; }
};
static_assert(OTABackendContract<MinimalBackend>);

// begin() must accept the one-argument form; a backend without the default
// ota_type argument breaks consumers that only pass the image size.
struct BackendWithoutDefaultOTAType {
  OTAResponseTypes begin(size_t image_size, OTAType ota_type) { return OTA_RESPONSE_OK; }
  void set_update_md5(const char *md5) {}
  OTAResponseTypes write(uint8_t *data, size_t len) { return OTA_RESPONSE_OK; }
  OTAResponseTypes end() { return OTA_RESPONSE_OK; }
  void abort() {}
  bool supports_compression() { return false; }
};
static_assert(!OTABackendContract<BackendWithoutDefaultOTAType>);

struct BackendMissingAbort {
  OTAResponseTypes begin(size_t image_size, OTAType ota_type = OTA_TYPE_UPDATE_APP) { return OTA_RESPONSE_OK; }
  void set_update_md5(const char *md5) {}
  OTAResponseTypes write(uint8_t *data, size_t len) { return OTA_RESPONSE_OK; }
  OTAResponseTypes end() { return OTA_RESPONSE_OK; }
  bool supports_compression() { return false; }
};
static_assert(!OTABackendContract<BackendMissingAbort>);

struct BackendWrongWriteReturn {
  OTAResponseTypes begin(size_t image_size, OTAType ota_type = OTA_TYPE_UPDATE_APP) { return OTA_RESPONSE_OK; }
  void set_update_md5(const char *md5) {}
  bool write(uint8_t *data, size_t len) { return true; }
  OTAResponseTypes end() { return OTA_RESPONSE_OK; }
  void abort() {}
  bool supports_compression() { return false; }
};
static_assert(!OTABackendContract<BackendWrongWriteReturn>);

TEST(OTABackendContract, MinimalBackendDrivesTheConsumerCallSequence) {
  // Exercise the surface the way ota_esphome does: begin, md5, write, end,
  // then an unconditional abort, which must be safe after end().
  MinimalBackend backend;
  uint8_t chunk[4] = {1, 2, 3, 4};
  EXPECT_EQ(backend.begin(sizeof(chunk)), OTA_RESPONSE_OK);
  backend.set_update_md5("00000000000000000000000000000000");
  EXPECT_EQ(backend.write(chunk, sizeof(chunk)), OTA_RESPONSE_OK);
  EXPECT_EQ(backend.end(), OTA_RESPONSE_OK);
  backend.abort();
  EXPECT_FALSE(backend.supports_compression());
}

}  // namespace esphome::ota::testing
