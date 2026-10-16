#pragma once
// nrf52 also defines USE_ZEPHYR but is excluded here -- see ota_backend_factory.h.
#if defined(USE_ZEPHYR) && !defined(USE_NRF52)
#include "ota_backend.h"

#include "esphome/components/md5/md5.h"

#include <cstddef>
#include <cstdint>

#ifdef USE_ZEPHYR_VARIANT_NATIVE_SIM
#include <string>
#else
#include <zephyr/dfu/flash_img.h>
#endif

namespace esphome::ota {

class ZephyrOTABackend final {
 public:
  OTAResponseTypes begin(size_t image_size, OTAType ota_type = OTA_TYPE_UPDATE_APP);
  void set_update_md5(const char *md5);
  OTAResponseTypes write(const uint8_t *data, size_t len);
  OTAResponseTypes end();
  void abort();
  static constexpr bool supports_compression() { return false; }
#if defined(USE_OTA_ZEPHYR_DIRECT_XIP) && !defined(USE_ZEPHYR_VARIANT_NATIVE_SIM)
  // True when slot1 is the slot currently executing -- the host must then send the
  // slot0-linked variant instead of re-sending what's running. See espota2.py's
  // SERVER_FEATURE_ACTIVE_SLOT_1.
  bool active_slot_is_secondary() const;
#endif

 protected:
  UpdateLock lock_;
  md5::MD5Digest md5_{};
  size_t expected_size_{0};
  size_t bytes_written_{0};
  uint8_t expected_md5_[16]{};
  bool md5_set_{false};

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
