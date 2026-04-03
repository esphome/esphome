#pragma once
#ifdef USE_ESP32
#include "ota_backend.h"

#include "esphome/components/md5/md5.h"
#include "esphome/core/defines.h"

#include <esp_ota_ops.h>

namespace esphome {
namespace ota {

class IDFOTABackend final {
 public:
  OTAResponseTypes begin(size_t image_size);
  void set_update_md5(const char *md5);
  OTAResponseTypes write(uint8_t *data, size_t len);
  OTAResponseTypes end();
  void abort();
  bool supports_compression() { return false; }

 private:
  esp_ota_handle_t update_handle_{0};
  const esp_partition_t *partition_;
  md5::MD5Digest md5_{};
  char expected_bin_md5_[32];
  bool md5_set_{false};
};

std::unique_ptr<IDFOTABackend> make_ota_backend();

}  // namespace ota
}  // namespace esphome
#endif  // USE_ESP32
