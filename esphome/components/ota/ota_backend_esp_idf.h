#pragma once
#ifdef USE_ESP32
#include "ota_backend.h"

#include "esphome/components/md5/md5.h"
#include "esphome/core/defines.h"

#include <esp_idf_version.h>
#include <esp_ota_ops.h>

// esp_ota_resume() (IDF 5.4.2+, backported to 5.3.3) provides a no-erase OTA
// handle, letting write() block-erase 64 KiB ahead of the write cursor
// (~4x faster than the per-sector lazy erase of OTA_WITH_SEQUENTIAL_WRITES,
// used as fallback on older IDF).
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 2) || \
    (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 3) && ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 4, 0))
#define USE_OTA_BLOCK_ERASE_AHEAD
#endif

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
  static constexpr bool supports_compression() { return false; }

 protected:
  UpdateLock lock_;
  OTAResponseTypes hold_lock_if_ok_(OTAResponseTypes result) {
    if (result == OTA_RESPONSE_OK)
      this->lock_.hold_until_reboot();
    return result;
  }
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
  OTAResponseTypes update_partition_table_();
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
  OTAResponseTypes write_app_(const uint8_t *data, size_t len);
#ifdef USE_OTA_BLOCK_ERASE_AHEAD
  OTAResponseTypes erase_ahead_(size_t len);
#endif
  OTAResponseTypes begin_app_(size_t image_size);
#ifdef USE_OTA_DELTA
  OTAResponseTypes begin_delta_(size_t image_size);
  OTAResponseTypes delta_write_(const uint8_t *data, size_t len);
  void delta_deinit_();
  static esp_err_t delta_read_cb_(uint8_t *buf, size_t size, int src_offset, void *user_data);
  static esp_err_t delta_write_cb_(const uint8_t *buf, size_t size, void *user_data);
#endif
#ifdef USE_OTA_SIGNED_VERIFICATION_MULTI_KEY
  // Accept an image signed by any key the running app trusts (up to 3 blocks),
  // so rotation and backup keys work. Fails closed. Covers app and bootloader.
  bool verify_signed_image_(const esp_partition_t *incoming);
#endif
  // Keep md5_ first since its digest_ is alignas(32) on DMA-SHA variants; md5_set_ stays last so buf_ packs tightly.
  md5::MD5Digest md5_{};
  esp_ota_handle_t update_handle_{0};
  const esp_partition_t *partition_{nullptr};
  size_t written_{0};  // Bytes handed to esp_ota_write()
#ifdef USE_OTA_BLOCK_ERASE_AHEAD
  size_t erased_end_{0};  // Erased up to this partition offset; must stay >= written_
#endif
  char expected_bin_md5_[32];
  bool md5_set_{false};
#ifdef USE_OTA_DELTA
  static constexpr size_t DELTA_HEADER_SIZE = 64;
  bool delta_{false};
  bool delta_header_ok_{false};
  uint8_t delta_header_[DELTA_HEADER_SIZE];
  size_t delta_header_len_{0};
  void *delta_handle_{nullptr};
  const esp_partition_t *delta_base_{nullptr};
  OTAResponseTypes delta_error_{OTA_RESPONSE_OK};
  static constexpr size_t DELTA_IO_SIZE = 4096;
  uint8_t *delta_rbuf_{nullptr};
  size_t delta_rbuf_start_{SIZE_MAX};
  uint8_t *delta_wbuf_{nullptr};
  size_t delta_wlen_{0};
  OTAResponseTypes delta_flush_();
#endif
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
