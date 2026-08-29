#pragma once
// nrf52 also defines USE_ZEPHYR but is excluded here -- see ota_backend_factory.h.
#if defined(USE_ZEPHYR) && !defined(USE_NRF52)
#include "ota_backend.h"

#include "esphome/components/sha256/sha256.h"

#include <cstddef>
#include <cstdint>

#ifdef USE_ZEPHYR_VARIANT_NATIVE_SIM
#include <string>
#else
#include <zephyr/dfu/flash_img.h>
#endif

namespace esphome::ota {

// SHA256 checksum only -- not MD5. NCS's nrf_security routes crypto through pluggable
// PSA drivers (Oberon by default), which don't implement MD5 at all (unlike SHA256).
class ZephyrOTABackend final {
 public:
  OTAResponseTypes begin(size_t image_size, OTAType ota_type = OTA_TYPE_UPDATE_APP);
  // Unused: supports_sha256_checksum() is true, but the (non-virtual) backend
  // interface is shared across platforms, so this still needs to exist.
  void set_update_md5(const char *md5) {}
  void set_update_sha256(const char *sha256);
  OTAResponseTypes write(const uint8_t *data, size_t len);
  OTAResponseTypes end();
  void abort();
  bool supports_compression() const { return false; }
  bool supports_sha256_checksum() const { return true; }
  // No MD5 fallback exists here (set_update_md5() is a no-op) -- silently skipping
  // verification would be worse than rejecting a client that can't negotiate SHA256.
  bool requires_sha256_checksum() const { return true; }
#if defined(USE_OTA_ZEPHYR_DIRECT_XIP) && !defined(USE_ZEPHYR_VARIANT_NATIVE_SIM)
  // True when slot1 is the slot currently executing -- the host must then send the
  // slot0-linked variant instead of re-sending what's running. See espota2.py's
  // SERVER_FEATURE_ACTIVE_SLOT_1.
  bool active_slot_is_secondary() const;
#endif

 protected:
  sha256::SHA256 sha256_{};
  size_t expected_size_{0};
  size_t bytes_written_{0};
  uint8_t expected_sha256_[32]{};
  bool sha256_set_{false};

#ifdef USE_ZEPHYR_VARIANT_NATIVE_SIM
  // native_sim has no real flash/bootloader -- OTA replaces the running host
  // executable in place and re-execs it, so staging happens on the host filesystem.
  std::string staging_path_;
  std::string final_path_;
  int fd_{-1};
#else
  // Real Zephyr hardware under MCUboot: stream the image straight into the
  // secondary (slot1) flash partition and let MCUboot swap it in on reboot.
  flash_img_context img_ctx_{};
  bool active_{false};
#ifdef USE_OTA_ZEPHYR_DIRECT_XIP
  // The slot NOT currently executing, chosen once in begin() and reused in end() --
  // direct-xip has no fixed "update slot", unlike swap modes' always-slot1 target.
  uint8_t target_area_id_{0};
#endif
#endif
};

std::unique_ptr<ZephyrOTABackend> make_ota_backend();

}  // namespace esphome::ota
#endif  // USE_ZEPHYR && !USE_NRF52
