#pragma once
#ifdef USE_LIBRETINY
#include "ota_backend.h"

#include "esphome/components/md5/md5.h"
#include "esphome/core/defines.h"

namespace esphome::ota {

class ArduinoLibreTinyOTABackend final {
 public:
  OTAResponseTypes begin(size_t image_size, OTAType ota_type = OTA_TYPE_UPDATE_APP);
  void set_update_md5(const char *md5);
  OTAResponseTypes write(uint8_t *data, size_t len);
  OTAResponseTypes end();
  void abort();
  static constexpr bool supports_compression() { return false; }

 private:
  // LibreTiny uses a full 512-byte UF2 block in place, and patches it there
  // when writing the OTA2 slot, so it never gets more than one byte less
  static constexpr size_t MAX_FEED = 511;

  bool feed_(const uint8_t *data, size_t len);

  UpdateLock lock_;
  md5::MD5Digest md5_{};
  uint8_t expected_md5_[16];
  // The last byte is held back until end() has checked the MD5: lt_ota_end(),
  // which Update.abort() runs too, marks the image bootable once every block is
  // written. LN882H cannot revert that mark, so this is its only guard.
  uint8_t pending_{0};
  bool has_pending_{false};
  bool md5_set_{false};
  bool wrote_{false};
};

std::unique_ptr<ArduinoLibreTinyOTABackend> make_ota_backend();

}  // namespace esphome::ota
#endif  // USE_LIBRETINY
