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
  // One UF2 block. LibreTiny patches a full block in place when writing the
  // OTA2 slot, so it only ever gets this copy, never the caller's buffer.
  static constexpr size_t BLOCK_SIZE = 512;

  bool write_block_();

  UpdateLock lock_;
  md5::MD5Digest md5_{};
  uint8_t expected_md5_[16];
  // The last block is held back until end(), so a failed MD5 check leaves
  // the image incomplete and LibreTiny does not mark it bootable.
  uint8_t block_[BLOCK_SIZE];
  size_t block_len_{0};
  bool md5_set_{false};
};

std::unique_ptr<ArduinoLibreTinyOTABackend> make_ota_backend();

}  // namespace esphome::ota
#endif  // USE_LIBRETINY
