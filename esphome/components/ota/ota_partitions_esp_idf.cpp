#ifdef USE_ESP32
#include "ota_backend_esp_idf.h"

#include "esphome/core/defines.h"

#ifdef USE_OTA_PARTITIONS
#include "esphome/components/watchdog/watchdog.h"
#include "esphome/core/log.h"

#include <esp_image_format.h>
#include <esp_ota_ops.h>
#include <nvs_flash.h>

#include <cinttypes>
#include <cstring>

namespace esphome::ota {

static const char *const TAG = "ota.idf";

static inline bool check_overlap(uint32_t a_offset, size_t a_size, uint32_t b_offset, size_t b_size) {
  return (a_offset + a_size > b_offset && b_offset + b_size > a_offset);
}

// Wraps esp_partition_find/_get/_next/_release. Returns nullptr if no APP partition at `address`
// is at least `min_size` bytes.
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

// Validates the staged partition table and picks the post-update boot slot. All non-destructive
// checks live here; the destructive write is in update_partition_table().
// Side effect: registers the live partition-table region as partition_table_part_ so the caller
// can write to it; abort() releases it on error.
OTAResponseTypes IDFOTABackend::validate_new_partition_table_(uint32_t running_app_offset, size_t running_app_size,
                                                              PartitionTablePlan &plan) {
  OTAResponseTypes validate_result = this->register_and_validate_partition_table_part_();
  if (validate_result != OTA_RESPONSE_OK) {
    return validate_result;
  }

  int num_partitions = 0;
  const esp_partition_info_t *new_partition_table = reinterpret_cast<const esp_partition_info_t *>(this->buf_);
  esp_err_t err = esp_partition_table_verify(new_partition_table, true, &num_partitions);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_partition_table_verify failed (new partition table) (err=0x%X)", err);
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
  }

  // esp_partition_table_verify does not catch a missing MD5 entry, but the bootloader refuses
  // to boot from a table without one.
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

  // Slot-selection policy when multiple slots can host the running app: pick the FIRST eligible
  // slot in table order, preferring the no-copy path (matching offset) over the copy path.
  // Deterministic and table-ordering-stable.
  int app_partitions_found = 0;
  int new_app_part_index = -1;
  int new_app_part_index_with_copy = -1;
  const esp_partition_t *app_copy_dest_part = nullptr;
  bool otadata_partition_found = false;
  bool otadata_overlap = false;
  bool nvs_partition_found = false;
  for (int i = 0; i < num_partitions; i++) {
    const esp_partition_info_t *new_part = &new_partition_table[i];
    if (new_part->type == ESP_PARTITION_TYPE_APP) {
      app_partitions_found++;
      if (new_part->pos.size >= running_app_size) {
        if (new_part->pos.offset == running_app_offset) {
          if (new_app_part_index == -1) {
            new_app_part_index = i;
          }
        } else if (new_app_part_index_with_copy == -1 &&
                   !check_overlap(running_app_offset, running_app_size, new_part->pos.offset, running_app_size)) {
          // esp_partition_copy writes into a registered partition; need one at this offset in the
          // current table.
          const esp_partition_t *p = find_app_partition_at(new_part->pos.offset, running_app_size);
          if (p != nullptr) {
            new_app_part_index_with_copy = i;
            app_copy_dest_part = p;
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
    // Most likely cause: the user picked the wrong migration .bin for their running app's size.
    // Rejecting here is non-destructive (no flash op has run yet); the user can safely retry with
    // a different .bin. Log enough info that they can pick the right method without guessing.
    ESP_LOGE(TAG,
             "The new partition table must contain a compatible app partition with:\n"
             "  size: at least %" PRIu32 " bytes (0x%" PRIX32 ")\n"
             "  address: one of",
             (uint32_t) running_app_size, (uint32_t) running_app_size);
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, nullptr);
    while (it != nullptr) {
      const esp_partition_t *partition = esp_partition_get(it);
      if (partition->size >= running_app_size) {
        ESP_LOGE(TAG, "    0x%" PRIX32, partition->address);
      }
      it = esp_partition_next(it);
    }
    esp_partition_iterator_release(it);
    ESP_LOGE(TAG, "Upload a different partition table. No flash content was modified.");
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
    // Unlikely, the otadata partition is before the start of the first app partition in most cases
    ESP_LOGE(TAG,
             "New otadata partition overlaps with the running app at address: 0x%" PRIX32 ", running app size: %" PRIu32
             " bytes",
             running_app_offset, (uint32_t) running_app_size);
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
  }

  if (new_app_part_index != -1) {
    plan.target_app_index = new_app_part_index;
    plan.copy_dest_part = nullptr;
  } else {
    plan.target_app_index = new_app_part_index_with_copy;
    plan.copy_dest_part = app_copy_dest_part;
  }
  return OTA_RESPONSE_OK;
}

OTAResponseTypes IDFOTABackend::update_partition_table() {
  if (this->buf_written_ == 0 || this->image_size_ != this->buf_written_) {
    ESP_LOGE(TAG, "Not enough data received");
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
  }

  // Without a valid running-app size we cannot compute overlap or copy bounds. zero indicates
  // esp_ota_get_running_partition() failed (e.g. cache unloaded by a previous aborted OTA).
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

  // ERROR severity so the warning shows up in default log filters; any failure past this point
  // can leave the device unbootable until it is recovered with a serial flash.
  ESP_LOGE(TAG, "Starting partition table update.\n"
                "  DO NOT REMOVE POWER until the device reboots successfully.\n"
                "  Loss of power during this operation may render the device\n"
                "  unable to boot until it is recovered via a serial flash.");

  // One guard over the whole critical section in case an IDF call takes longer than expected on
  // some chip variant.
  watchdog::WatchdogManager watchdog(15000);

  esp_err_t err;
  const esp_partition_info_t *new_partition_table = reinterpret_cast<const esp_partition_info_t *>(this->buf_);

  if (plan.copy_dest_part != nullptr) {
    // Resolve the source via running_app_offset rather than esp_ota_get_running_partition() in
    // case a prior aborted partition-table OTA called esp_partition_unload_all() in this boot,
    // which leaves esp_ota_get_running_partition() returning nullptr.
    const esp_partition_t *running_app_part = find_app_partition_at(running_app_offset, running_app_size);
    if (running_app_part == nullptr) {
      ESP_LOGE(TAG, "Cannot resolve running app partition at address 0x%" PRIX32, running_app_offset);
      return OTA_RESPONSE_ERROR_PARTITION_TABLE_UPDATE;
    }
    ESP_LOGD(TAG, "Copying running app from 0x%X to 0x%X (size: 0x%X)", running_app_part->address,
             plan.copy_dest_part->address, running_app_size);
    err = esp_partition_copy(plan.copy_dest_part, 0, running_app_part, 0, running_app_size);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_partition_copy failed (err=0x%X)", err);
      return OTA_RESPONSE_ERROR_PARTITION_TABLE_UPDATE;
    }
  }

  // Deinit NVS only just before the first destructive write so verify/copy failure paths return
  // with NVS still functional. From this point on, components that hold open NVS handles
  // (e.g. preferences) will fail with ESP_ERR_NVS_INVALID_HANDLE on success or failure;
  // nvs_flash_init() can re-init the subsystem but cannot revive existing handles. On the
  // success path the device reboots immediately afterwards so this doesn't matter; on the
  // failure path the user must reboot the device before retrying.
  nvs_flash_deinit();

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
    esp_ota_abort(this->update_handle_);
    this->update_handle_ = 0;
    ESP_LOGE(TAG, "esp_ota_write failed (err=0x%X)", err);
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_UPDATE;
  }
  err = esp_ota_end(this->update_handle_);
  this->update_handle_ = 0;  // esp_ota_end releases the handle internally
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ota_end failed (err=0x%X)", err);
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_UPDATE;
  }
  // unload first, then null the member pointer; if abort() ran between the two steps it would
  // see a freed pointer. esp_partition_unload_all() invalidates partition_table_part_ too, so
  // an explicit deregister would be redundant.
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
  return OTA_RESPONSE_OK;
}

OTAResponseTypes IDFOTABackend::register_and_validate_partition_table_part_() {
  esp_err_t err = esp_partition_register_external(
      nullptr, ESP_PRIMARY_PARTITION_TABLE_OFFSET, ESP_PARTITION_TABLE_SIZE, "PrimaryPrtTable",
      ESP_PARTITION_TYPE_PARTITION_TABLE, ESP_PARTITION_SUBTYPE_PARTITION_TABLE_PRIMARY, &this->partition_table_part_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_partition_register_external failed (partition table) (err=0x%X)", err);
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
  }

  int num_partitions = 0;
  const esp_partition_info_t *existing_partition_table = nullptr;
  esp_partition_mmap_handle_t partition_table_map;
  err = esp_partition_mmap(this->partition_table_part_, 0, ESP_PARTITION_TABLE_MAX_LEN, ESP_PARTITION_MMAP_DATA,
                           reinterpret_cast<const void **>(&existing_partition_table), &partition_table_map);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_partition_mmap failed (partition table) (err=0x%X)", err);
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
  }
  err = esp_partition_table_verify(existing_partition_table, true, &num_partitions);
  esp_partition_munmap(partition_table_map);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_partition_table_verify failed (existing partition table) (err=0x%X)", err);
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
  }
  return OTA_RESPONSE_OK;
}

// Process-scoped cache. Cannot be a backend member: backends are per-connection but the cache
// must outlive a connection that called esp_partition_unload_all(), after which
// esp_ota_get_running_partition() no longer returns valid data.
static bool s_running_app_initialized = false;
static uint32_t s_running_app_cached_offset = 0;
static size_t s_running_app_cached_size = 0;

// Flag-gated rather than size==0 so a failed first call doesn't poison the cache.
void get_running_app_position(uint32_t &offset, size_t &size) {
  if (!s_running_app_initialized) {
    const esp_partition_t *running_app_part = esp_ota_get_running_partition();
    if (running_app_part == nullptr || running_app_part->erase_size == 0) {
      // Surface zeros without committing to the cache so a later call has a chance to succeed.
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
    // Round up to a full flash sector so the copy spans complete erase blocks.
    pending_size = ((pending_size + running_app_part->erase_size - 1) / running_app_part->erase_size) *
                   running_app_part->erase_size;

    s_running_app_cached_offset = pending_offset;
    s_running_app_cached_size = pending_size;
    s_running_app_initialized = true;
  }

  offset = s_running_app_cached_offset;
  size = s_running_app_cached_size;
}

}  // namespace esphome::ota

#endif  // USE_OTA_PARTITIONS
#endif  // USE_ESP32
