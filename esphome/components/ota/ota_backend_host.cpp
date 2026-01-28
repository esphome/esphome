#ifdef USE_HOST
#include "ota_backend_host.h"

#include "esphome/core/defines.h"

namespace esphome::ota {

// Stub implementation - OTA is not supported on host platform.
// All methods return error codes to allow compilation of configs with OTA triggers.

std::unique_ptr<ota::OTABackend> make_ota_backend() { return make_unique<ota::HostOTABackend>(); }

OTAResponseTypes HostOTABackend::begin(size_t image_size) { return OTA_RESPONSE_ERROR_UPDATE_PREPARE; }

void HostOTABackend::set_update_md5(const char *expected_md5) {}

OTAResponseTypes HostOTABackend::write(uint8_t *data, size_t len) { return OTA_RESPONSE_ERROR_WRITING_FLASH; }

OTAResponseTypes HostOTABackend::end() { return OTA_RESPONSE_ERROR_UPDATE_END; }

void HostOTABackend::abort() {}

}  // namespace esphome::ota
#endif
