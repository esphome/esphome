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

#ifdef USE_NOISE_SPARE_EPHEMERAL
static constexpr size_t PRIVATE_KEY_SIZE = SPARE_EPHEMERAL_KEY_SIZE;
static constexpr size_t PUBLIC_KEY_SIZE = SPARE_EPHEMERAL_KEY_SIZE;
uint8_t spare_ephemeral[SPARE_EPHEMERAL_SIZE];  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void prepare_spare_ephemeral() {
  uint8_t *private_key = spare_ephemeral;
  uint8_t *public_key = spare_ephemeral + PRIVATE_KEY_SIZE;
  // Same steps as noise-c's curve25519 keygen; the clamp sets the ready bit,
  // a failure wipes the slot so the handshake generates its own key
  if (!random_bytes(private_key, PRIVATE_KEY_SIZE)) {
    sodium_memzero(spare_ephemeral, sizeof(spare_ephemeral));
    return;
  }
  private_key[0] &= 0xF8;
  private_key[PRIVATE_KEY_SIZE - 1] = (private_key[PRIVATE_KEY_SIZE - 1] & 0x7F) | 0x40;
  if (crypto_scalarmult_curve25519_base(public_key, private_key) != 0) {
    sodium_memzero(spare_ephemeral, sizeof(spare_ephemeral));
  }
}

int consume_spare_ephemeral(NoiseHandshakeState *state) {
  if (!has_spare_ephemeral()) {
    return 0;
  }
  // noise-c keeps its own copy, so the slot is wiped either way
  int err = noise_handshakestate_set_local_ephemeral(state, spare_ephemeral, PRIVATE_KEY_SIZE,
                                                     spare_ephemeral + PRIVATE_KEY_SIZE, PUBLIC_KEY_SIZE);
  sodium_memzero(spare_ephemeral, sizeof(spare_ephemeral));
  return err;
}
#endif  // USE_NOISE_SPARE_EPHEMERAL

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
