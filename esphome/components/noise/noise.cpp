#include "noise.h"
#ifdef USE_NOISE
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cstring>

#include <noise/protocol.h>
#include <sodium.h>

#ifdef USE_ESP8266
#include <pgmspace.h>
#endif

namespace esphome::noise {

static const char *const TAG = "noise";

void NoiseContext::load_psk(psk_t &out) const {
  if (this->psk_ == nullptr) {
    out.fill(0);
    return;
  }
  progmem_memcpy(out.data(), this->psk_, out.size());
}

#ifdef USE_API_NOISE
static uint8_t spare_ephemeral[EPHEMERAL_KEYPAIR_SIZE];  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static bool spare_ephemeral_ready = false;               // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

bool has_spare_ephemeral() { return spare_ephemeral_ready; }

void prepare_spare_ephemeral() {
  uint8_t *private_key = spare_ephemeral;
  uint8_t *public_key = spare_ephemeral + EPHEMERAL_PRIVATE_KEY_SIZE;
  // Same generation as noise-c's curve25519 backend: random bytes, X25519
  // clamping, then the public key. A random source failure leaves the slot
  // empty; the handshake then generates its own key.
  if (!random_bytes(private_key, EPHEMERAL_PRIVATE_KEY_SIZE)) {
    return;
  }
  private_key[0] &= 0xF8;
  private_key[EPHEMERAL_PRIVATE_KEY_SIZE - 1] = (private_key[EPHEMERAL_PRIVATE_KEY_SIZE - 1] & 0x7F) | 0x40;
  crypto_scalarmult_curve25519_base(public_key, private_key);
  spare_ephemeral_ready = true;
}

bool take_spare_ephemeral(ephemeral_keypair_t &out) {
  if (!spare_ephemeral_ready) {
    return false;
  }
  std::memcpy(out.data(), spare_ephemeral, EPHEMERAL_KEYPAIR_SIZE);
  sodium_memzero(spare_ephemeral, EPHEMERAL_KEYPAIR_SIZE);
  spare_ephemeral_ready = false;
  return true;
}
#endif  // USE_API_NOISE

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

const LogString *reject_reason_for(int err) {
  return err == NOISE_ERROR_MAC_FAILURE ? LOG_STR("Handshake MAC failure") : LOG_STR("Handshake error");
}

size_t format_reject_payload(uint8_t *buf, size_t capacity, const LogString *reason) {
  if (capacity == 0) {
    // A caller bug; the MAC_FAILURE_PAYLOAD_SIZE static_asserts at the call
    // sites make this unreachable, kept as cheap memory safety
    ESP_LOGVV(TAG, "Reject buffer has no capacity");
    return 0;
  }
  buf[0] = HANDSHAKE_STATUS_REJECT;
#ifdef USE_STORE_LOG_STR_IN_FLASH
  // On ESP8266 with flash strings, we need to use PROGMEM-aware functions
  size_t reason_len = strlen_P(reinterpret_cast<PGM_P>(reason));
  reason_len = std::min(reason_len, capacity - 1);
  if (reason_len > 0) {
    memcpy_P(buf + 1, reinterpret_cast<PGM_P>(reason), reason_len);
  }
#else
  const char *reason_str = LOG_STR_ARG(reason);
  size_t reason_len = strlen(reason_str);
  reason_len = std::min(reason_len, capacity - 1);
  if (reason_len > 0) {
    // NOLINTNEXTLINE(bugprone-not-null-terminated-result) - binary protocol, not a C string
    std::memcpy(buf + 1, reason_str, reason_len);
  }
#endif
  return reason_len + 1;
}

}  // namespace esphome::noise
#endif  // USE_NOISE
