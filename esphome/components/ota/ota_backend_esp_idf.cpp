#ifdef USE_ESP32
#include "ota_backend_esp_idf.h"

#include "esphome/components/md5/md5.h"
#include "esphome/components/watchdog/watchdog.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"

#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include <spi_flash_mmap.h>

#ifdef USE_OTA_PARTITIONS
#include <esp_image_format.h>
#include <nvs_flash.h>
#endif

namespace esphome::ota {

static const char *const TAG = "ota.idf";

std::unique_ptr<IDFOTABackend> make_ota_backend() { return make_unique<IDFOTABackend>(); }

#ifdef USE_OTA_PARTITIONS
OTAResponseTypes IDFOTABackend::begin(size_t image_size, ota::OTAType ota_type) {
  this->ota_type_ = ota_type;
  if (this->ota_type_ == ota::OTA_TYPE_UPDATE_PARTITION_TABLE) {
    // Partition table images produced by gen_esp32part.py are padded with 0xFF and an MD5 entry to
    // exactly ESP_PARTITION_TABLE_MAX_LEN bytes. Reject anything else: an undersized image would
    // leave trailing bytes from the previous table in place after the partial write, and an
    // oversized image cannot fit in the reserved region. This is stricter than verify alone.
    if (image_size != ESP_PARTITION_TABLE_MAX_LEN) {
      ESP_LOGE(TAG, "Wrong partition table size: expected %u bytes, got %zu", ESP_PARTITION_TABLE_MAX_LEN, image_size);
      return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
    }
    memset(this->buf_, 0xFF, sizeof this->buf_);
    this->buf_written_ = 0;
    this->image_size_ = image_size;
    this->md5_.init();
    return OTA_RESPONSE_OK;
  }
  if (this->ota_type_ != ota::OTA_TYPE_UPDATE_APP) {
    return OTA_RESPONSE_ERROR_UNSUPPORTED_OTA_TYPE;
  }
#else
OTAResponseTypes IDFOTABackend::begin(size_t image_size) {
#endif
#ifdef USE_OTA_ROLLBACK
  // If we're starting an OTA, the current boot is good enough - mark it valid
  // to prevent rollback and allow the OTA to proceed even if the safe mode
  // timer hasn't expired yet.
  esp_ota_mark_app_valid_cancel_rollback();
#endif

  this->partition_ = esp_ota_get_next_update_partition(nullptr);
  if (this->partition_ == nullptr) {
    return OTA_RESPONSE_ERROR_NO_UPDATE_PARTITION;
  }

  watchdog::WatchdogManager watchdog(15000);
  esp_err_t err = esp_ota_begin(this->partition_, image_size, &this->update_handle_);

  if (err != ESP_OK) {
    esp_ota_abort(this->update_handle_);
    this->update_handle_ = 0;
    if (err == ESP_ERR_INVALID_SIZE) {
      return OTA_RESPONSE_ERROR_ESP32_NOT_ENOUGH_SPACE;
    } else if (err == ESP_ERR_FLASH_OP_TIMEOUT || err == ESP_ERR_FLASH_OP_FAIL) {
      return OTA_RESPONSE_ERROR_WRITING_FLASH;
    }
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }
  this->md5_.init();
  return OTA_RESPONSE_OK;
}

void IDFOTABackend::set_update_md5(const char *expected_md5) {
  memcpy(this->expected_bin_md5_, expected_md5, 32);
  this->md5_set_ = true;
}

OTAResponseTypes IDFOTABackend::write(uint8_t *data, size_t len) {
#ifdef USE_OTA_PARTITIONS
  if (this->ota_type_ == ota::OTA_TYPE_UPDATE_PARTITION_TABLE) {
    if (len > PARTITION_TABLE_BUFFER_SIZE - this->buf_written_) {
      ESP_LOGE(TAG, "Wrong partition table size");
      return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
    }
    memcpy(this->buf_ + this->buf_written_, data, len);
    this->buf_written_ += len;
    this->md5_.add(data, len);
    return OTA_RESPONSE_OK;
  }
  if (this->ota_type_ != ota::OTA_TYPE_UPDATE_APP) {
    return OTA_RESPONSE_ERROR_UNSUPPORTED_OTA_TYPE;
  }
#endif
  esp_err_t err = esp_ota_write(this->update_handle_, data, len);
  this->md5_.add(data, len);
  if (err != ESP_OK) {
    if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
      return OTA_RESPONSE_ERROR_MAGIC;
    } else if (err == ESP_ERR_FLASH_OP_TIMEOUT || err == ESP_ERR_FLASH_OP_FAIL) {
      return OTA_RESPONSE_ERROR_WRITING_FLASH;
    }
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }
  return OTA_RESPONSE_OK;
}

OTAResponseTypes IDFOTABackend::end() {
  if (this->md5_set_) {
    this->md5_.calculate();
    if (!this->md5_.equals_hex(this->expected_bin_md5_)) {
      this->abort();
      return OTA_RESPONSE_ERROR_MD5_MISMATCH;
    }
  }
#ifdef USE_OTA_PARTITIONS
  if (this->ota_type_ == ota::OTA_TYPE_UPDATE_PARTITION_TABLE) {
    return this->update_partition_table();
  }
  if (this->ota_type_ != ota::OTA_TYPE_UPDATE_APP) {
    return OTA_RESPONSE_ERROR_UNSUPPORTED_OTA_TYPE;
  }
#endif
  esp_err_t err = esp_ota_end(this->update_handle_);
  this->update_handle_ = 0;
  if (err == ESP_OK) {
    err = esp_ota_set_boot_partition(this->partition_);
    if (err == ESP_OK) {
      return OTA_RESPONSE_OK;
    }
  }
  if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
#ifdef USE_OTA_SIGNED_VERIFICATION
    ESP_LOGE(TAG, "OTA validation failed (err=0x%X) - possible signature verification failure", err);
    return OTA_RESPONSE_ERROR_SIGNATURE_INVALID;
#else
    return OTA_RESPONSE_ERROR_UPDATE_END;
#endif
  }
  if (err == ESP_ERR_FLASH_OP_TIMEOUT || err == ESP_ERR_FLASH_OP_FAIL) {
    return OTA_RESPONSE_ERROR_WRITING_FLASH;
  }
  return OTA_RESPONSE_ERROR_UNKNOWN;
}

void IDFOTABackend::abort() {
#ifdef USE_OTA_PARTITIONS
  if (this->partition_table_part_ != nullptr) {
    esp_partition_deregister_external(this->partition_table_part_);
    this->partition_table_part_ = nullptr;
  }
#endif
  // Always tear down any open OTA handle. update_partition_table() opens a handle internally to
  // write the new partition table; if esp_ota_write/esp_ota_end fail mid-flight, the handle must
  // be released here so it isn't leaked. esp_ota_abort with handle 0 returns ESP_ERR_INVALID_ARG
  // harmlessly, so the unconditional call is safe whether or not we're mid-update.
  esp_ota_abort(this->update_handle_);
  this->update_handle_ = 0;
}

#ifdef USE_OTA_PARTITIONS
static inline bool check_overlap(uint32_t a_offset, size_t a_size, uint32_t b_offset, size_t b_size) {
  return (a_offset + a_size > b_offset && b_offset + b_size > a_offset);
}

// Find the first registered APP partition whose address matches `address` and whose size is at least
// `min_size`. Returns nullptr when no match exists. Encapsulates the iterator + release pattern so
// callers don't have to repeat (and correctly handle) the find/get/next/release dance.
static const esp_partition_t *find_app_partition_at(uint32_t address, size_t min_size) {
  const esp_partition_t *found = nullptr;
  esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, nullptr);
  while (it != nullptr) {
    const esp_partition_t *p = esp_partition_get(it);
    if (p->address == address && p->size >= min_size) {
      found = p;
      break;
    }
    it = esp_partition_next(it);
  }
  esp_partition_iterator_release(it);
  return found;
}

// RAII helper for the destructive section of update_partition_table(). nvs_flash_deinit() is
// called immediately before the first partition-table write so that earlier failure paths leave
// NVS functional; this guard re-initializes NVS on every early return past that point so any
// component still running after a failed OTA can keep using NVS. The success path disarms the
// guard before returning because the device reboots immediately afterwards and reinit would only
// churn the partition cache.
namespace {
struct NvsReinitGuard {
  bool armed{true};
  ~NvsReinitGuard() {
    if (armed) {
      nvs_flash_init();
    }
  }
};
}  // namespace

// Validate the new partition table image staged in ``buf_`` and pick the slot the running app
// will boot from after the update. Performs all non-destructive checks; the destructive write
// is in ``update_partition_table()``. Side-effect: registers the live partition-table region
// as ``partition_table_part_`` so the caller can write to it; ``abort()`` releases it on error.
OTAResponseTypes IDFOTABackend::validate_new_partition_table_(uint32_t running_app_offset, size_t running_app_size,
                                                              PartitionTablePlan &plan) {
  // Register the live primary partition table as an external partition so we can mmap it for
  // verification and later issue esp_ota_begin/esp_ota_write against it.
  esp_err_t err = esp_partition_register_external(
      nullptr, ESP_PRIMARY_PARTITION_TABLE_OFFSET, ESP_PARTITION_TABLE_SIZE, "PrimaryPrtTable",
      ESP_PARTITION_TYPE_PARTITION_TABLE, ESP_PARTITION_SUBTYPE_PARTITION_TABLE_PRIMARY, &this->partition_table_part_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_partition_register_external failed (err=0x%X)", err);
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
  }

  // Verify existing partition table
  int num_partitions = 0;
  const esp_partition_info_t *existing_partition_table = nullptr;
  esp_partition_mmap_handle_t partition_table_map;
  err = esp_partition_mmap(this->partition_table_part_, 0, ESP_PARTITION_TABLE_MAX_LEN, ESP_PARTITION_MMAP_DATA,
                           reinterpret_cast<const void **>(&existing_partition_table), &partition_table_map);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_partition_mmap failed (err=0x%X)", err);
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
  }
  err = esp_partition_table_verify(existing_partition_table, true, &num_partitions);
  esp_partition_munmap(partition_table_map);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_partition_table_verify failed (existing partition table) (err=0x%X)", err);
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
  }

  // Verify new partition table. esp_partition_table_verify expects ESP_PARTITION_TABLE_MAX_LEN
  // bytes; ``buf_`` is sized to that exactly.
  const esp_partition_info_t *new_partition_table = reinterpret_cast<const esp_partition_info_t *>(this->buf_);
  err = esp_partition_table_verify(new_partition_table, true, &num_partitions);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_partition_table_verify failed (new partition table) (err=0x%X)", err);
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
  }

  // Check for missing checksum entry. esp_partition_table_verify does not fail in this case and
  // the ESP would not boot after the update.
  bool checksum_found = false;
  for (size_t i = 0; i < ESP_PARTITION_TABLE_MAX_ENTRIES; i++) {
    if (new_partition_table[i].magic == ESP_PARTITION_MAGIC_MD5) {
      checksum_found = true;
      break;
    }
  }
  if (!checksum_found) {
    ESP_LOGE(TAG, "New partition table has no checksum");
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
  }

  // Walk the new table once, populating: the chosen target app slot, presence of otadata/nvs,
  // and otadata-vs-running-app overlap. Selection policy when multiple app slots can host the
  // running app: pick the FIRST eligible slot in table order. The no-copy path (offsets already
  // match) is preferred over the copy path; within each path we lock in the first match and stop
  // searching. This keeps the choice deterministic and table-ordering-stable.
  int app_partitions_found = 0;
  int new_app_part_index = -1;
  int new_app_part_index_with_copy = -1;
  const esp_partition_t *app_copy_source_part = nullptr;
  bool otadata_partition_found = false;
  bool otadata_overlap = false;
  bool nvs_partition_found = false;
  for (int i = 0; i < num_partitions; i++) {
    const esp_partition_info_t *new_part = &new_partition_table[i];
    if (new_part->type == ESP_PARTITION_TYPE_APP) {
      app_partitions_found++;
      if (new_part->pos.size >= running_app_size) {
        if (new_part->pos.offset == running_app_offset) {
          // No-copy path: same offset as running app, first match wins.
          if (new_app_part_index == -1) {
            new_app_part_index = i;
          }
        } else if (new_app_part_index_with_copy == -1 &&
                   !check_overlap(running_app_offset, running_app_size, new_part->pos.offset, running_app_size)) {
          // Copy path: needs a registered source partition in the *current* table at the new slot's offset.
          const esp_partition_t *p = find_app_partition_at(new_part->pos.offset, running_app_size);
          if (p != nullptr) {
            new_app_part_index_with_copy = i;
            app_copy_source_part = p;
          }
        }
      }
    } else if (new_part->type == ESP_PARTITION_TYPE_DATA) {
      if (new_part->subtype == ESP_PARTITION_SUBTYPE_DATA_OTA) {
        otadata_partition_found = true;
        otadata_overlap = check_overlap(running_app_offset, running_app_size, new_part->pos.offset, new_part->pos.size);
      } else if (new_part->subtype == ESP_PARTITION_SUBTYPE_DATA_NVS &&
                 strncmp(reinterpret_cast<const char *>(new_part->label), "nvs", sizeof(new_part->label)) == 0) {
        nvs_partition_found = true;
      }
    }
  }

  if (new_app_part_index == -1 && new_app_part_index_with_copy == -1) {
    ESP_LOGE(TAG, "No compatible app partition found in the new partition table");
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
  }
  if (app_partitions_found < 2) {
    ESP_LOGE(TAG, "New partition table needs at least 2 app partitions, found %d", app_partitions_found);
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
  }
  if (!otadata_partition_found) {
    ESP_LOGE(TAG, "New partition table is missing the required otadata partition");
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
  }
  if (!nvs_partition_found) {
    ESP_LOGE(TAG, "New partition table is missing the required nvs partition");
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
  }
  if (otadata_overlap) {
    ESP_LOGE(TAG, "New otadata partition overlaps with running app");
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
  }

  // No-copy preferred; copy path only when no-copy slot was unavailable.
  if (new_app_part_index != -1) {
    plan.target_app_index = new_app_part_index;
    plan.copy_source_part = nullptr;
  } else {
    plan.target_app_index = new_app_part_index_with_copy;
    plan.copy_source_part = app_copy_source_part;
  }
  return OTA_RESPONSE_OK;
}

OTAResponseTypes IDFOTABackend::update_partition_table() {
  if (this->buf_written_ == 0 || this->image_size_ != this->buf_written_) {
    ESP_LOGE(TAG, "Not enough data received");
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
  }

  // Get running app partition and used size. A zero size means we couldn't determine the running
  // app (e.g., esp_ota_get_running_partition() returned nullptr after a previous aborted partition
  // table OTA called esp_partition_unload_all()). Without a valid size we cannot safely compute
  // overlap or copy bounds, so fail before any flash operation.
  uint32_t running_app_offset;
  size_t running_app_size;
  get_running_app_position(running_app_offset, running_app_size);
  if (running_app_size == 0) {
    ESP_LOGE(TAG, "Failed to determine running app position");
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
  }

  PartitionTablePlan plan;
  OTAResponseTypes validate_result = this->validate_new_partition_table_(running_app_offset, running_app_size, plan);
  if (validate_result != OTA_RESPONSE_OK) {
    return validate_result;
  }

  // Past this point any failure (power loss, watchdog reset, write error after the table has been
  // partially erased) can leave the device unable to boot. Logged at ERROR severity so the message
  // is visible in default log filters.
  ESP_LOGE(TAG, "Starting partition table update.\n"
                "  DO NOT REMOVE POWER until the device reboots successfully.\n"
                "  Loss of power during this operation may permanently brick the device.");

  // Hold the watchdog open for the entire critical section: optional app copy, partition-table
  // erase/write, and boot partition selection. None of the steps below should yield long enough
  // to require a refresh, but bundling them under a single guard avoids spurious resets if the
  // underlying ESP-IDF calls take longer than expected on a given chip variant.
  watchdog::WatchdogManager watchdog(15000);

  esp_err_t err;
  const esp_partition_info_t *new_partition_table = reinterpret_cast<const esp_partition_info_t *>(this->buf_);

  // Copy the running app partition to new position if needed.
  // esp_ota_get_running_partition() is still valid here (we have not yet called
  // esp_partition_unload_all()) and returns the same partition that find_app_partition_at would
  // have located, without an extra iterator walk.
  if (plan.copy_source_part != nullptr) {
    const esp_partition_t *running_app_part = esp_ota_get_running_partition();
    ESP_LOGD(TAG, "Copying running app from 0x%X to 0x%X (size: 0x%X)", running_app_part->address,
             plan.copy_source_part->address, running_app_size);

    err = esp_partition_copy(plan.copy_source_part, 0, running_app_part, 0, running_app_size);

    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_partition_copy failed (err=0x%X)", err);
      return OTA_RESPONSE_ERROR_PARTITION_TABLE_UPDATE;
    }
  }

  // Deinitialize NVS just before the first destructive write to the partition-table region. Doing
  // this here (instead of earlier) means that any failure path in the verify or copy phases above
  // returns with NVS still functional, so other components on the device aren't broken until reboot.
  // The RAII guard re-initializes NVS on every early-return below; the success path disarms it
  // immediately before returning, since the device reboots right after.
  nvs_flash_deinit();
  NvsReinitGuard nvs_guard;

  // Update the partition table
  err = esp_ota_begin(this->partition_table_part_, ESP_PARTITION_TABLE_MAX_LEN, &this->update_handle_);
  if (err != ESP_OK) {
    esp_ota_abort(this->update_handle_);
    this->update_handle_ = 0;
    ESP_LOGE(TAG, "esp_ota_begin failed (err=0x%X)", err);
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_UPDATE;
  }
  err = esp_ota_write(this->update_handle_, this->buf_, ESP_PARTITION_TABLE_MAX_LEN);
  if (err != ESP_OK) {
    // Release the handle eagerly; abort() would also do this, but cleaning up locally keeps the
    // partial-write failure path self-contained.
    esp_ota_abort(this->update_handle_);
    this->update_handle_ = 0;
    ESP_LOGE(TAG, "esp_ota_write failed (err=0x%X)", err);
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_UPDATE;
  }
  err = esp_ota_end(this->update_handle_);
  this->update_handle_ = 0;  // esp_ota_end releases the handle internally regardless of result
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ota_end failed (err=0x%X)", err);
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_UPDATE;
  }
  // esp_partition_unload_all() invalidates every cached partition entry, including the externally
  // registered `partition_table_part_`, so the explicit deregister call is redundant. Do the
  // unload first, then null the member pointer so it never dangles past invalidation; if abort()
  // were ever to observe an in-between state, it would see a non-null but freed pointer and crash.
  esp_partition_unload_all();
  this->partition_table_part_ = nullptr;

  // Write otadata to set the new boot partition
  const esp_partition_info_t *new_part = &new_partition_table[plan.target_app_index];
  const esp_partition_t *new_boot_partition = find_app_partition_at(new_part->pos.offset, 0);
  if (new_boot_partition == nullptr) {
    ESP_LOGE(TAG, "Selected app partition not found after partition table update");
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_UPDATE;
  }
  ESP_LOGD(TAG, "Setting next boot partition to 0x%X", new_boot_partition->address);
  err = esp_ota_set_boot_partition(new_boot_partition);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (err=0x%X)", err);
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_UPDATE;
  }
  nvs_guard.armed = false;
  return OTA_RESPONSE_OK;
}

// Process-scoped cache of the running app's flash position. Cannot live on IDFOTABackend
// because the backend is created/destroyed per OTA connection, while the cached values must
// survive across connections: once a previously aborted partition-table OTA has called
// esp_partition_unload_all(), esp_ota_get_running_partition() no longer returns valid data,
// so we have to remember the answer from the first successful call. The running app does not
// move within a boot, so a single capture is valid for the process lifetime.
static bool s_running_app_initialized = false;
static uint32_t s_running_app_cached_offset = 0;
static size_t s_running_app_cached_size = 0;

void get_running_app_position(uint32_t &offset, size_t &size) {
  // Returns the start address and the used length (rounded up to flash sectors) of the running app.
  // The ``s_running_app_initialized`` flag (rather than ``size == 0``) gates the cache so a failed
  // first call does not poison it; the next caller retries. Values are written atomically only
  // after the full computation succeeds.
  if (!s_running_app_initialized) {
    const esp_partition_t *running_app_part = esp_ota_get_running_partition();
    if (running_app_part == nullptr || running_app_part->erase_size == 0) {
      // Cannot determine the running app right now; surface zeros without committing to the cache
      // so a later call has a chance to succeed.
      offset = 0;
      size = 0;
      return;
    }

    uint32_t pending_offset = running_app_part->address;
    size_t pending_size = running_app_part->size;

    const esp_partition_pos_t running_app_pos = {
        .offset = running_app_part->address,
        .size = running_app_part->size,
    };
    esp_image_metadata_t image_metadata = {};
    image_metadata.start_addr = running_app_part->address;
    if (esp_image_verify(ESP_IMAGE_VERIFY_SILENT, &running_app_pos, &image_metadata) == ESP_OK &&
        image_metadata.image_len < running_app_part->size) {
      pending_size = image_metadata.image_len;
    }
    // Round up to flash sector size so the copy spans complete erase blocks.
    pending_size = ((pending_size + running_app_part->erase_size - 1) / running_app_part->erase_size) *
                   running_app_part->erase_size;

    s_running_app_cached_offset = pending_offset;
    s_running_app_cached_size = pending_size;
    s_running_app_initialized = true;
  }

  offset = s_running_app_cached_offset;
  size = s_running_app_cached_size;
}
#endif

}  // namespace esphome::ota
#endif  // USE_ESP32
