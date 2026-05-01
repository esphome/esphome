#pragma once
#ifdef USE_ESP32
#include "ota_backend.h"

#include "esphome/components/md5/md5.h"
#include "esphome/core/defines.h"

#include <esp_ota_ops.h>

namespace esphome::ota {

#ifdef USE_OTA_PARTITIONS
// Dedicated staging buffer size for the new partition table image. Must be at least
// ESP_PARTITION_TABLE_MAX_LEN (0xC00) so the entire partition table fits before verification.
// Kept separate from any OTA chunk-transfer buffer to avoid coupling unrelated sizes.
static constexpr size_t PARTITION_TABLE_BUFFER_SIZE = ESP_PARTITION_TABLE_MAX_LEN;  // 0xC00

void get_running_app_position(uint32_t &offset, size_t &size);
#endif

class IDFOTABackend final {
 public:
#ifdef USE_OTA_PARTITIONS
  OTAResponseTypes begin(size_t image_size, ota::OTAType ota_type = ota::OTA_TYPE_UPDATE_APP);
#else
  OTAResponseTypes begin(size_t image_size);
#endif
  void set_update_md5(const char *md5);
  OTAResponseTypes write(uint8_t *data, size_t len);
  OTAResponseTypes end();
  void abort();
  bool supports_compression() { return false; }

 protected:
#ifdef USE_OTA_PARTITIONS
  OTAResponseTypes update_partition_table();
#endif

 private:
  esp_ota_handle_t update_handle_{0};
  const esp_partition_t *partition_;
  md5::MD5Digest md5_{};
  char expected_bin_md5_[32];
  bool md5_set_{false};
#ifdef USE_OTA_PARTITIONS
  // Place the byte buffer first so it sits immediately after the preceding `bool md5_set_`,
  // eliminating the 3-byte alignment padding that an int-sized member would otherwise force.
  // Remaining members are 4-byte-aligned and pack tightly after the buffer.
  uint8_t buf_[PARTITION_TABLE_BUFFER_SIZE];
  size_t buf_written_{0};
  size_t image_size_{0};
  const esp_partition_t *partition_table_part_{nullptr};
  ota::OTAType ota_type_{ota::OTA_TYPE_UPDATE_APP};
#endif
};

std::unique_ptr<IDFOTABackend> make_ota_backend();

}  // namespace esphome::ota
#endif  // USE_ESP32
