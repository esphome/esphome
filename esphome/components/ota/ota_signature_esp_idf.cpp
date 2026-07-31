#ifdef USE_ESP32
#include "ota_backend_esp_idf.h"

#ifdef USE_OTA_SIGNED_VERIFICATION_MULTI_KEY
#include "esphome/core/log.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <esp_image_format.h>
#include <esp_partition.h>
#include <esp_rom_crc.h>

#include <mbedtls/md.h>
#include <mbedtls/rsa.h>
#include <mbedtls/sha256.h>

namespace esphome::ota {

static const char *const TAG = "ota.idf";

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

// A trusted public key is identified by the SHA-256 of its 776-byte key region,
// exactly as the ROM computes it -- this is what "the running app trusts" means.
using KeyDigest = std::array<uint8_t, SHA256_BYTES>;

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
  return mbedtls_sha256(block + OFFSET_KEY, KEY_REGION_LEN, out.data(), /*is224=*/0) == 0;
}

// The offset of the signature sector: the app length rounded up to 4 KiB.
bool signature_sector_offset(const esp_partition_t *part, size_t &out_offset) {
  esp_partition_pos_t pos{.offset = part->address, .size = part->size};
  esp_image_metadata_t meta{};
  if (esp_image_get_metadata(&pos, &meta) != ESP_OK) {
    return false;
  }
  out_offset = (meta.image_len + SIG_SECTOR_ALIGN - 1) & ~(SIG_SECTOR_ALIGN - 1);
  return out_offset + SIG_BLOCK_SIZE <= part->size;
}

// SHA-256 over the 4 KiB-padded image, i.e. everything the signature covers.
// Returns false on a read or hash error so a hash failure is not later
// misreported as a signature mismatch.
bool image_digest(const esp_partition_t *part, size_t image_padded_len, uint8_t *out) {
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  bool ok = mbedtls_sha256_starts(&ctx, /*is224=*/0) == 0;
  uint8_t buf[512];
  for (size_t off = 0; ok && off < image_padded_len; off += sizeof(buf)) {
    size_t chunk = std::min(sizeof(buf), image_padded_len - off);
    if (esp_partition_read(part, off, buf, chunk) != ESP_OK || mbedtls_sha256_update(&ctx, buf, chunk) != 0) {
      ok = false;
    }
  }
  if (ok) {
    ok = mbedtls_sha256_finish(&ctx, out) == 0;
  }
  mbedtls_sha256_free(&ctx);
  return ok;
}

// Collect the key digests of every valid signature block in an image. Returns
// the count found (0 on read error), writing up to SIG_BLOCK_MAX_COUNT digests.
// The caller supplies the block buffer so this shares one with verify_signed_
// image_ rather than stacking a second 1.2 KB frame during verification.
size_t collect_key_digests(const esp_partition_t *part, size_t sector_offset, uint8_t *block,
                           std::array<KeyDigest, SIG_BLOCK_MAX_COUNT> &out) {
  size_t count = 0;
  for (size_t i = 0; i < SIG_BLOCK_MAX_COUNT; i++) {
    size_t off = sector_offset + i * SIG_BLOCK_SIZE;
    if (off + SIG_BLOCK_SIZE > part->size || esp_partition_read(part, off, block, SIG_BLOCK_SIZE) != ESP_OK) {
      break;
    }
    if (block_is_valid(block) && key_digest_of(block, out[count])) {
      count++;
    }
  }
  return count;
}

// Verify one RSA-PSS-3072-SHA256 signature block over the image digest. The
// block's modulus and signature are stored little-endian and reversed here.
bool rsa_pss_verify(const uint8_t *block, const uint8_t *digest) {
  uint8_t modulus_be[RSA_3072_BYTES];
  uint8_t signature_be[RSA_3072_BYTES];
  for (size_t i = 0; i < RSA_3072_BYTES; i++) {
    modulus_be[i] = block[OFFSET_MODULUS + RSA_3072_BYTES - 1 - i];
    signature_be[i] = block[OFFSET_SIGNATURE + RSA_3072_BYTES - 1 - i];
  }
  uint32_t exponent_le;
  memcpy(&exponent_le, block + OFFSET_EXPONENT, sizeof(exponent_le));
  uint8_t exponent_be[4] = {static_cast<uint8_t>(exponent_le >> 24), static_cast<uint8_t>(exponent_le >> 16),
                            static_cast<uint8_t>(exponent_le >> 8), static_cast<uint8_t>(exponent_le)};

  mbedtls_rsa_context rsa;
  mbedtls_rsa_init(&rsa);
  bool verified = false;
  if (mbedtls_rsa_import_raw(&rsa, modulus_be, sizeof(modulus_be), nullptr, 0, nullptr, 0, nullptr, 0, exponent_be,
                             sizeof(exponent_be)) == 0 &&
      mbedtls_rsa_complete(&rsa) == 0 &&
      mbedtls_rsa_set_padding(&rsa, MBEDTLS_RSA_PKCS_V21, MBEDTLS_MD_SHA256) == 0) {
    verified = mbedtls_rsa_rsassa_pss_verify(&rsa, MBEDTLS_MD_SHA256, SHA256_BYTES, digest, signature_be) == 0;
  }
  mbedtls_rsa_free(&rsa);
  return verified;
}

}  // namespace

bool IDFOTABackend::verify_signed_image_(const esp_partition_t *incoming) {
  const esp_partition_t *running = esp_ota_get_running_partition();
  if (running == nullptr) {
    ESP_LOGE(TAG, "Signature check: no running partition");
    return false;
  }

  // One block buffer, reused for both the trusted-key scan and the incoming
  // scan below, to keep the verify path's stack footprint down.
  uint8_t block[SIG_BLOCK_SIZE];

  // The set of keys the running app trusts: the keys in its own signature
  // blocks. A device only ever accepts an image sharing one of these.
  size_t running_sector;
  std::array<KeyDigest, SIG_BLOCK_MAX_COUNT> trusted;
  size_t trusted_count = 0;
  if (signature_sector_offset(running, running_sector)) {
    trusted_count = collect_key_digests(running, running_sector, block, trusted);
  }
  if (trusted_count == 0) {
    ESP_LOGE(TAG, "Signature check: running app has no trusted keys");
    return false;
  }

  size_t incoming_sector;
  if (!signature_sector_offset(incoming, incoming_sector)) {
    ESP_LOGE(TAG, "Signature check: cannot locate incoming signature sector");
    return false;
  }
  uint8_t digest[SHA256_BYTES];
  if (!image_digest(incoming, incoming_sector, digest)) {
    ESP_LOGE(TAG, "Signature check: cannot hash incoming image");
    return false;
  }

  // Accept if any incoming block is signed by a trusted key AND its signature
  // verifies over the image. Iterating all blocks (not just the first) is the
  // whole point -- it lets a bridge/backup key in a later block be the match.
  for (size_t i = 0; i < SIG_BLOCK_MAX_COUNT; i++) {
    size_t off = incoming_sector + i * SIG_BLOCK_SIZE;
    if (off + SIG_BLOCK_SIZE > incoming->size || esp_partition_read(incoming, off, block, SIG_BLOCK_SIZE) != ESP_OK) {
      break;
    }
    if (!block_is_valid(block)) {
      continue;
    }
    KeyDigest incoming_key;
    if (!key_digest_of(block, incoming_key)) {
      ESP_LOGE(TAG, "Signature check: failed to hash incoming key in block %zu", i);
      return false;
    }
    bool trusted_key = false;
    for (size_t t = 0; t < trusted_count; t++) {
      if (incoming_key == trusted[t]) {
        trusted_key = true;
        break;
      }
    }
    if (!trusted_key) {
      continue;
    }
    if (rsa_pss_verify(block, digest)) {
      ESP_LOGD(TAG, "Signature check: verified with trusted key in block %zu", i);
      return true;
    }
    ESP_LOGW(TAG, "Signature check: trusted key in block %zu failed to verify", i);
  }

  ESP_LOGE(TAG, "Signature check: no trusted key produced a valid signature");
  return false;
}

}  // namespace esphome::ota

#endif  // USE_OTA_SIGNED_VERIFICATION_MULTI_KEY
#endif  // USE_ESP32
