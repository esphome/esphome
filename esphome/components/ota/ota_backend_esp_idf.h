#pragma once
#include "esphome/core/defines.h"
#ifdef USE_ESP_IDF

#include "ota_component.h"
#include "ota_backend.h"
#include <esp_ota_ops.h>
#include "esphome/components/md5/md5.h"

namespace esphome {
namespace ota {

class IDFOTABackend : public OTABackend {
 public:
  OTAResponseTypes begin(size_t image_size) override;
  void set_update_md5(const char *md5) override;
  OTAResponseTypes write(uint8_t *data, size_t len) override;
  OTAResponseTypes end() override;
  void abort() override;
  bool supports_compression() override { return false; }

 private:
  esp_ota_handle_t update_handle_{0};
  const esp_partition_t *partition_;
  md5::MD5Digest md5_{};
  char expected_bin_md5_[32];
};

}  // namespace ota
}  // namespace esphome
#endif
