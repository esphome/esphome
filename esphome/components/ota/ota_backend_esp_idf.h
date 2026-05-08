#pragma once
#ifdef USE_ESP32
#include "ota_backend.h"

#include "esphome/components/md5/md5.h"
#include "esphome/core/defines.h"

#include <esp_ota_ops.h>

namespace esphome::ota {

#ifdef USE_OTA_PARTITIONS
// Staging buffer holds the entire partition table for verification before any flash op.
static constexpr size_t PARTITION_TABLE_BUFFER_SIZE = ESP_PARTITION_TABLE_MAX_LEN;  // 0xC00

void get_running_app_position(uint32_t &offset, size_t &size);
#endif

class IDFOTABackend final {
 public:
  OTAResponseTypes begin(size_t image_size, ota::OTAType ota_type = ota::OTA_TYPE_UPDATE_APP);
  void set_update_md5(const char *md5);
  OTAResponseTypes write(uint8_t *data, size_t len);
  OTAResponseTypes end();
  void abort();
  bool supports_compression() { return false; }

 protected:
#ifdef USE_OTA_PARTITIONS
  // copy_dest_part non-null means the running app must be copied INTO this slot of the current
  // table before the new partition table is committed. The destination is in the current table
  // because that's where esp_partition_copy can write; once the new table replaces it, the same
  // flash region becomes target_app_index in the new table.
  struct PartitionTablePlan {
    int target_app_index{-1};
    const esp_partition_t *copy_dest_part{nullptr};
  };

  OTAResponseTypes validate_new_partition_table_(uint32_t running_app_offset, size_t running_app_size,
                                                 PartitionTablePlan &plan);
  OTAResponseTypes update_partition_table();
  OTAResponseTypes register_and_validate_partition_table_part_();
  // Defined in ota_bootloader_esp_idf.cpp:
  OTAResponseTypes register_and_validate_bootloader_part_();
  OTAResponseTypes prepare_bootloader_update_(size_t image_size);
  OTAResponseTypes setup_bootloader_staging_();
  OTAResponseTypes finalize_bootloader_update_(esp_err_t ota_end_err);

  // The OTA types that flow through esp_ota_begin/write/end. Partition-table updates take a
  // separate code path that buffers the table in RAM and never touches the OTA handle.
  bool is_app_or_bootloader_update_() const {
    return this->ota_type_ == ota::OTA_TYPE_UPDATE_APP || this->ota_type_ == ota::OTA_TYPE_UPDATE_BOOTLOADER;
  }
#endif

 private:
  esp_ota_handle_t update_handle_{0};
  const esp_partition_t *partition_;
  md5::MD5Digest md5_{};
  char expected_bin_md5_[32];
  bool md5_set_{false};
#ifdef USE_OTA_PARTITIONS
  // Buffer first so it packs tightly after the preceding `bool md5_set_` with no alignment
  // padding. Only resident during an active OTA: the backend is constructed per connection and
  // destroyed on cleanup_connection_().
  uint8_t buf_[PARTITION_TABLE_BUFFER_SIZE];
  size_t buf_written_{0};
  size_t image_size_{0};
  const esp_partition_t *partition_table_part_{nullptr};
  const esp_partition_t *bootloader_part_{nullptr};
  ota::OTAType ota_type_{ota::OTA_TYPE_UPDATE_APP};
#endif
};

std::unique_ptr<IDFOTABackend> make_ota_backend();

}  // namespace esphome::ota
#endif  // USE_ESP32
