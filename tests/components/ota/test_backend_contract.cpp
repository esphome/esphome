// Pins the OTA backend contract concept so the surface it enforces cannot
// drift unnoticed: the build's real backend and a minimal conforming type
// must satisfy it, and a type missing a method or returning the wrong type
// must not.

#include "esphome/components/ota/ota_backend.h"
#include "esphome/components/ota/ota_backend_host.h"

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

// Each negative case derives from MinimalBackend and breaks exactly one
// requirement; the declaration in the derived struct hides the conforming
// one from the base.

// begin() without the default ota_type argument breaks consumers that only
// pass the image size.
struct BackendWithoutDefaultOTAType : MinimalBackend {
  OTAResponseTypes begin(size_t image_size, OTAType ota_type) { return OTA_RESPONSE_OK; }
};
static_assert(!OTABackendContract<BackendWithoutDefaultOTAType>);

struct BackendMissingAbort : MinimalBackend {
  void abort() = delete;
};
static_assert(!OTABackendContract<BackendMissingAbort>);

struct BackendWrongWriteReturn : MinimalBackend {
  bool write(uint8_t *data, size_t len) { return true; }
};
static_assert(!OTABackendContract<BackendWrongWriteReturn>);

// Pin the build's real backend, not just local mocks: the unit test harness
// builds for the host platform, so this is the same check the factory's
// static_assert performs in a firmware compile.
#ifdef USE_HOST
static_assert(OTABackendContract<HostOTABackend>);
#endif

}  // namespace esphome::ota::testing
