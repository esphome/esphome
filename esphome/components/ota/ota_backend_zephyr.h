#pragma once
// nrf52 also defines USE_ZEPHYR but is excluded here -- see ota_backend_factory.h.
#if defined(USE_ZEPHYR) && !defined(USE_NRF52)
#include "ota_backend.h"

#include "esphome/components/md5/md5.h"

#include <cstddef>
#include <cstdint>

#include <zephyr/dfu/flash_img.h>

namespace esphome::ota {

class ZephyrOTABackend final {
 public:
  OTAResponseTypes begin(size_t image_size, OTAType ota_type = OTA_TYPE_UPDATE_APP);
  void set_update_md5(const char *md5);
  OTAResponseTypes write(const uint8_t *data, size_t len);
  OTAResponseTypes end();
  void abort();
  static constexpr bool supports_compression() { return false; }

 protected:
  UpdateLock lock_;
  md5::MD5Digest md5_{};
  size_t expected_size_{0};
  size_t bytes_written_{0};
  uint8_t expected_md5_[16]{};
  bool md5_set_{false};

  // Streams the image into the secondary (slot1) partition; MCUboot swaps it in on reboot.
  flash_img_context img_ctx_{};
  bool active_{false};
};

std::unique_ptr<ZephyrOTABackend> make_ota_backend();

}  // namespace esphome::ota
#endif  // USE_ZEPHYR && !USE_NRF52
