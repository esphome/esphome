#include "noise.h"
#ifdef USE_NOISE
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <noise/protocol.h>

namespace esphome::noise {

static const char *const TAG = "noise";

const LogString *noise_err_to_logstr(int err) {
  if (err == NOISE_ERROR_NO_MEMORY)
    return LOG_STR("NO_MEMORY");
  if (err == NOISE_ERROR_UNKNOWN_ID)
    return LOG_STR("UNKNOWN_ID");
  if (err == NOISE_ERROR_UNKNOWN_NAME)
    return LOG_STR("UNKNOWN_NAME");
  if (err == NOISE_ERROR_MAC_FAILURE)
    return LOG_STR("MAC_FAILURE");
  if (err == NOISE_ERROR_NOT_APPLICABLE)
    return LOG_STR("NOT_APPLICABLE");
  if (err == NOISE_ERROR_SYSTEM)
    return LOG_STR("SYSTEM");
  if (err == NOISE_ERROR_REMOTE_KEY_REQUIRED)
    return LOG_STR("REMOTE_KEY_REQUIRED");
  if (err == NOISE_ERROR_LOCAL_KEY_REQUIRED)
    return LOG_STR("LOCAL_KEY_REQUIRED");
  if (err == NOISE_ERROR_PSK_REQUIRED)
    return LOG_STR("PSK_REQUIRED");
  if (err == NOISE_ERROR_INVALID_LENGTH)
    return LOG_STR("INVALID_LENGTH");
  if (err == NOISE_ERROR_INVALID_PARAM)
    return LOG_STR("INVALID_PARAM");
  if (err == NOISE_ERROR_INVALID_STATE)
    return LOG_STR("INVALID_STATE");
  if (err == NOISE_ERROR_INVALID_NONCE)
    return LOG_STR("INVALID_NONCE");
  if (err == NOISE_ERROR_INVALID_PRIVATE_KEY)
    return LOG_STR("INVALID_PRIVATE_KEY");
  if (err == NOISE_ERROR_INVALID_PUBLIC_KEY)
    return LOG_STR("INVALID_PUBLIC_KEY");
  if (err == NOISE_ERROR_INVALID_FORMAT)
    return LOG_STR("INVALID_FORMAT");
  if (err == NOISE_ERROR_INVALID_SIGNATURE)
    return LOG_STR("INVALID_SIGNATURE");
  return LOG_STR("UNKNOWN");
}

extern "C" {
// declare how noise generates random bytes (here with a good HWRNG based on the RF system)
void noise_rand_bytes(void *output, size_t len) {
  if (!esphome::random_bytes(reinterpret_cast<uint8_t *>(output), len)) {
    ESP_LOGE(TAG, "Acquiring random bytes failed; rebooting");
    arch_restart();
  }
}
}

}  // namespace esphome::noise
#endif  // USE_NOISE
