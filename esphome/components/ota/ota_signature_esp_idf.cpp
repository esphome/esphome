#ifdef USE_ESP32
#include "ota_backend_esp_idf.h"

#ifdef USE_OTA_SIGNED_VERIFICATION_MULTI_KEY
#include "esphome/components/watchdog/watchdog.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <new>
#include <esp_image_format.h>
#include <esp_partition.h>
#include <esp_rom_crc.h>

#include <esp_idf_version.h>
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
// mbedtls 4.0 (IDF 6.0) made the legacy mbedtls_rsa_*/mbedtls_sha256_* headers
// private. Use the PSA Crypto API instead, like the sha256 component does. PSA
// crypto is auto-initialized by ESP-IDF at startup (esp_psa_crypto_init.c,
// priority 104), so no psa_crypto_init() call is needed.
#define USE_OTA_SIG_PSA
#include "ota_rsa_der.h"
#include <psa/crypto.h>
#else
#include <mbedtls/md.h>
#include <mbedtls/rsa.h>
#include <mbedtls/sha256.h>
#endif

namespace esphome::ota {

static const char *const TAG = "ota";

// Route the "Signature check: " prefix (and its per-block form) through one
// shared format string each, so the prefix is pooled once by the linker instead
// of duplicated at every call site. The level macro is forwarded so compile-time
// log-level stripping still applies.
#define OTA_IDF_SIG_LOG(level, msg) level(TAG, "Signature check: %s", msg)
#define OTA_IDF_SIG_LOG_BLOCK(level, i, msg) level(TAG, "Signature check: block %zu: %s", static_cast<size_t>(i), msg)

// Secure Boot v2 RSA-3072 signature block, as written by espsecure and stored
// in the 4 KiB sector following the (4 KiB-padded) app image. All bignum
// fields are byte-reversed to little-endian for the RSA accelerator; software
// verification reverses them back. See the espsecure "<BBxx32s384sI384sI384s"
// packing for the authoritative layout.
namespace {
constexpr uint8_t SIG_BLOCK_MAGIC = 0xE7;
constexpr uint8_t SIG_BLOCK_VERSION_RSA = 0x02;
constexpr size_t SIG_BLOCK_SIZE = 1216;
constexpr size_t SIG_SECTOR_ALIGN = 4096;
constexpr size_t SIG_BLOCK_MAX_COUNT = 3;
constexpr size_t RSA_3072_BYTES = 384;
constexpr size_t SHA256_BYTES = 32;

constexpr size_t OFFSET_KEY = 36;         // start of the hashed public-key region
constexpr size_t KEY_REGION_LEN = 776;    // n[384] + e[4] + rinv[384] + m[4]
constexpr size_t OFFSET_MODULUS = 36;     // n[384], little-endian
constexpr size_t OFFSET_EXPONENT = 420;   // e, uint32 little-endian
constexpr size_t OFFSET_SIGNATURE = 812;  // signature[384], little-endian
constexpr size_t OFFSET_CRC = 1196;       // crc32 over bytes [0, 1196)

// A public key is identified by the SHA-256 of its 776-byte key region, exactly
// as the ROM computes it. The trusted set is compiled into the app from the
// config's verification_keys (esp32 signed_ota codegen) -- an immutable anchor
// that, unlike the appendable signature sector, an OTA cannot enlarge.
using KeyDigest = std::array<uint8_t, SHA256_BYTES>;
constexpr uint8_t TRUSTED_KEY_DIGESTS[OTA_TRUSTED_KEY_COUNT][SHA256_BYTES] = OTA_TRUSTED_KEY_DIGESTS;

// A block is structurally valid if the magic, version, and CRC all check out.
// The CRC covers everything before it and uses the same ROM routine the
// bootloader validates the block with, so the check matches byte-for-byte.
bool block_is_valid(const uint8_t *block) {
  if (block[0] != SIG_BLOCK_MAGIC || block[1] != SIG_BLOCK_VERSION_RSA) {
    return false;
  }
  uint32_t stored_crc;
  memcpy(&stored_crc, block + OFFSET_CRC, sizeof(stored_crc));
  return esp_rom_crc32_le(0, block, OFFSET_CRC) == stored_crc;
}

bool key_digest_of(const uint8_t *block, KeyDigest &out) {
#ifdef USE_OTA_SIG_PSA
  size_t out_len = 0;
  return psa_hash_compute(PSA_ALG_SHA_256, block + OFFSET_KEY, KEY_REGION_LEN, out.data(), out.size(), &out_len) ==
             PSA_SUCCESS &&
         out_len == out.size();
#else
  return mbedtls_sha256(block + OFFSET_KEY, KEY_REGION_LEN, out.data(), /*is224=*/0) == 0;
#endif
}

// The offset of the signature sector: the app length rounded up to 4 KiB.
bool signature_sector_offset(const esp_partition_t *part, size_t &out_offset) {
  esp_partition_pos_t pos{.offset = part->address, .size = part->size};
  esp_image_metadata_t meta{};
  if (esp_image_get_metadata(&pos, &meta) != ESP_OK) {
    return false;
  }
  // Bound the image length before rounding up so a crafted header can't
  // overflow the addition; the image plus its signature sector must fit.
  if (meta.image_len > part->size) {
    return false;
  }
  out_offset = (meta.image_len + SIG_SECTOR_ALIGN - 1) & ~(SIG_SECTOR_ALIGN - 1);
  return out_offset + SIG_BLOCK_SIZE <= part->size;
}

// SHA-256 over the 4 KiB-padded image, i.e. everything the signature covers.
// Returns false on a read or hash error so a hash failure is not later
// misreported as a signature mismatch.
bool image_digest(const esp_partition_t *part, size_t image_padded_len, uint8_t *out) {
#ifdef USE_OTA_SIG_PSA
  psa_hash_operation_t ctx = PSA_HASH_OPERATION_INIT;
  bool ok = psa_hash_setup(&ctx, PSA_ALG_SHA_256) == PSA_SUCCESS;
#else
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  bool ok = mbedtls_sha256_starts(&ctx, /*is224=*/0) == 0;
#endif
  uint8_t buf[512];
  for (size_t off = 0; ok && off < image_padded_len; off += sizeof(buf)) {
    size_t chunk = std::min(sizeof(buf), image_padded_len - off);
    if (esp_partition_read(part, off, buf, chunk) != ESP_OK) {
      ok = false;
      break;
    }
#ifdef USE_OTA_SIG_PSA
    ok = psa_hash_update(&ctx, buf, chunk) == PSA_SUCCESS;
#else
    ok = mbedtls_sha256_update(&ctx, buf, chunk) == 0;
#endif
  }
#ifdef USE_OTA_SIG_PSA
  size_t out_len = 0;
  if (ok) {
    ok = psa_hash_finish(&ctx, out, SHA256_BYTES, &out_len) == PSA_SUCCESS && out_len == SHA256_BYTES;
  }
  // A no-op once the operation has been finished
  psa_hash_abort(&ctx);
#else
  if (ok) {
    ok = mbedtls_sha256_finish(&ctx, out) == 0;
  }
  mbedtls_sha256_free(&ctx);
#endif
  return ok;
}

// Verify one RSA-PSS-3072-SHA256 signature block over the image digest. The
// block's modulus and signature are stored little-endian; reverse them in place
// -- block is the caller's scratch buffer, overwritten on the next iteration --
// rather than stacking a second 384-byte copy of each bignum.

bool rsa_pss_verify(uint8_t *block, const uint8_t *digest) {
  std::reverse(block + OFFSET_MODULUS, block + OFFSET_MODULUS + RSA_3072_BYTES);
  std::reverse(block + OFFSET_SIGNATURE, block + OFFSET_SIGNATURE + RSA_3072_BYTES);
  uint32_t exponent_le;
  memcpy(&exponent_le, block + OFFSET_EXPONENT, sizeof(exponent_le));
  uint8_t exponent_be[4] = {static_cast<uint8_t>(exponent_le >> 24), static_cast<uint8_t>(exponent_le >> 16),
                            static_cast<uint8_t>(exponent_le >> 8), static_cast<uint8_t>(exponent_le)};

#ifdef USE_OTA_SIG_PSA
  static_assert(RSA_3072_BYTES == RSA_3072_MODULUS_BYTES, "signature block and DER encoder disagree on modulus size");
  uint8_t der[RSA_DER_PUBKEY_MAX];
  const size_t der_len = rsa_der_public_key(block + OFFSET_MODULUS, exponent_be, sizeof(exponent_be), der, sizeof(der));
  psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
  psa_set_key_type(&attr, PSA_KEY_TYPE_RSA_PUBLIC_KEY);
  psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_VERIFY_HASH);
  // ANY_SALT preserves the salt-length acceptance of mbedtls_rsa_rsassa_pss_verify(),
  // which this replaces; espsecure signs with a 32-byte salt. TF-PSA-Crypto defines
  // PSA_WANT_ALG_RSA_PSS_ANY_SALT from PSA_WANT_ALG_RSA_PSS, which IDF enables.
  psa_set_key_algorithm(&attr, PSA_ALG_RSA_PSS_ANY_SALT(PSA_ALG_SHA_256));
  mbedtls_svc_key_id_t key = MBEDTLS_SVC_KEY_ID_INIT;
  const bool key_ok = der_len != 0 && psa_import_key(&attr, der, der_len, &key) == PSA_SUCCESS;
#else
  mbedtls_rsa_context rsa;
  mbedtls_rsa_init(&rsa);
  const bool key_ok = mbedtls_rsa_import_raw(&rsa, block + OFFSET_MODULUS, RSA_3072_BYTES, nullptr, 0, nullptr, 0,
                                             nullptr, 0, exponent_be, sizeof(exponent_be)) == 0 &&
                      mbedtls_rsa_complete(&rsa) == 0 &&
                      mbedtls_rsa_set_padding(&rsa, MBEDTLS_RSA_PKCS_V21, MBEDTLS_MD_SHA256) == 0;
#endif
  bool verified = false;
  if (!key_ok) {
    // A setup/allocation failure (e.g. OOM right after the download) is not a
    // signature mismatch -- log it distinctly so it isn't read as "wrong key".
    OTA_IDF_SIG_LOG(ESP_LOGE, "RSA key setup failed");
  } else {
#ifdef USE_OTA_SIG_PSA
    verified = psa_verify_hash(key, PSA_ALG_RSA_PSS_ANY_SALT(PSA_ALG_SHA_256), digest, SHA256_BYTES,
                               block + OFFSET_SIGNATURE, RSA_3072_BYTES) == PSA_SUCCESS;
#else
    verified =
        mbedtls_rsa_rsassa_pss_verify(&rsa, MBEDTLS_MD_SHA256, SHA256_BYTES, digest, block + OFFSET_SIGNATURE) == 0;
#endif
  }
#ifdef USE_OTA_SIG_PSA
  if (key_ok) {
    psa_destroy_key(key);
  }
#else
  mbedtls_rsa_free(&rsa);
#endif
  return verified;
}

}  // namespace

bool IDFOTABackend::verify_signed_image_(const esp_partition_t *incoming) {
  // Verification re-hashes the full image (after esp_ota_end already did one
  // pass), which can approach the task WDT budget on a large app. Extend it for
  // the duration, mirroring the erase budget in begin().
  const uint32_t verify_budget_ms = 15000 + (incoming->size >> 10) * 10;
  watchdog::WatchdogManager watchdog(verify_budget_ms);

  size_t incoming_sector;
  if (!signature_sector_offset(incoming, incoming_sector)) {
    OTA_IDF_SIG_LOG(ESP_LOGE, "cannot locate incoming signature sector");
    return false;
  }
  uint8_t digest[SHA256_BYTES];
  if (!image_digest(incoming, incoming_sector, digest)) {
    OTA_IDF_SIG_LOG(ESP_LOGE, "cannot hash incoming image");
    return false;
  }

  // Accept if any incoming block is signed by a compiled-in trusted key AND its
  // signature verifies over the image. Iterating all blocks (not just the
  // first) is the whole point -- it lets a bridge/backup key in a later block
  // be the match. The trust check is against the immutable compiled-in set, so
  // extra (self-signed) blocks an attacker appends carry keys we simply ignore.
  // Heap-allocate the 1216-byte block for the duration of verification: this
  // runs mid-OTA on the loop task, on top of the caller's live 1 KB OTA buffer
  // and mbedtls's own ~1 KB verify scratch, so keeping it off the stack widens
  // a thin margin. One short-lived allocation right before reboot is not the
  // fragmentation pattern the project guards against. nothrow so an OOM here
  // fails closed like every other error path, rather than aborting.
  std::unique_ptr<uint8_t[]> block(new (std::nothrow) uint8_t[SIG_BLOCK_SIZE]);
  if (!block) {
    OTA_IDF_SIG_LOG(ESP_LOGE, "out of memory");
    return false;
  }
  bool any_valid_block = false;
  for (size_t i = 0; i < SIG_BLOCK_MAX_COUNT; i++) {
    size_t off = incoming_sector + i * SIG_BLOCK_SIZE;
    if (off + SIG_BLOCK_SIZE > incoming->size) {
      break;  // partition has no room for another block; done scanning
    }
    // A read fault is not "no trusted key" -- fail closed with a distinct error.
    if (esp_partition_read(incoming, off, block.get(), SIG_BLOCK_SIZE) != ESP_OK) {
      OTA_IDF_SIG_LOG_BLOCK(ESP_LOGE, i, "unreadable");
      return false;
    }
    if (!block_is_valid(block.get())) {
      OTA_IDF_SIG_LOG_BLOCK(ESP_LOGD, i, "absent or malformed");
      continue;
    }
    any_valid_block = true;
    KeyDigest incoming_key;
    if (!key_digest_of(block.get(), incoming_key)) {
      OTA_IDF_SIG_LOG_BLOCK(ESP_LOGE, i, "key hash failed");
      return false;
    }
    bool trusted_key = false;
    for (const auto &trusted : TRUSTED_KEY_DIGESTS) {
      if (memcmp(incoming_key.data(), trusted, SHA256_BYTES) == 0) {
        trusted_key = true;
        break;
      }
    }
    if (!trusted_key) {
      OTA_IDF_SIG_LOG_BLOCK(ESP_LOGW, i, "signed by an untrusted key");
      continue;
    }
    if (rsa_pss_verify(block.get(), digest)) {
      OTA_IDF_SIG_LOG_BLOCK(ESP_LOGD, i, "verified with a trusted key");
      return true;
    }
    OTA_IDF_SIG_LOG_BLOCK(ESP_LOGW, i, "trusted key failed to verify");
  }

  // Separate "not signed at all" from "signed by an untrusted key" -- the former
  // otherwise reads as the latter on a device that only logs at INFO.
  if (!any_valid_block) {
    OTA_IDF_SIG_LOG(ESP_LOGE, "image has no signature block");
  } else {
    OTA_IDF_SIG_LOG(ESP_LOGE, "no trusted key produced a valid signature");
  }
  return false;
}

}  // namespace esphome::ota

#endif  // USE_OTA_SIGNED_VERIFICATION_MULTI_KEY
#endif  // USE_ESP32
