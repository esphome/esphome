#include "tsdb.h"

#include <cinttypes>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <ctime>
#include <string>

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "esp_err.h"
#include "esp_littlefs.h"
#include "esp_partition.h"
#include "esp_tsdb.h"

namespace esphome::tsdb {

ESPHOME_LOG_TAG(TAG, "tsdb");

// ESP_PARTITION_SUBTYPE_DATA_LITTLEFS (0x83), missing from some IDF 5.x headers.
static constexpr uint8_t LITTLEFS_SUBTYPE = 0x83;
static constexpr uint32_t DROP_LOG_EVERY = 10;
static constexpr uint32_t SATURATION_LOG_EVERY = 100;
static constexpr uint32_t REOPEN_RETRY_MS = 300000;

static_assert(tsdbmath::BLOCK_SIZE == TSDB_BLOCK_SIZE, "esp_tsdb changed TSDB_BLOCK_SIZE");
static_assert(tsdbmath::BLOCK_HEADER_SIZE == TSDB_BLOCK_HEADER_SIZE, "esp_tsdb changed the block header");
static_assert(tsdbmath::records_per_block(4) == (TSDB_BLOCK_SIZE - TSDB_BLOCK_HEADER_SIZE) / (4 + 4 * 2),
              "esp_tsdb changed the record geometry");
static_assert(tsdbmath::max_records_for_bytes(3072, 4) == TSDB_CALC_MAX_RECORDS(3072, 4),
              "esp_tsdb changed TSDB_CALC_MAX_RECORDS()");
static_assert(sizeof(tsdb_header_t) == tsdbmath::HEADER_BYTES, "esp_tsdb changed sizeof(tsdb_header_t)");
static_assert(tsdbmath::HEADER_BYTES == TSDB_V4_EXT_NAMES_OFFSET, "esp_tsdb changed the header size");
static_assert(offsetof(tsdb_header_t, index_stride) + sizeof(uint32_t) <= tsdbmath::HEADER_BYTES,
              "esp_tsdb moved index_stride out of the header");

static bool partition_is_blank(const esp_partition_t *partition) {
  static constexpr size_t CHUNK = 256;
  uint8_t buffer[CHUNK];
  for (uint32_t offset = 0; offset < partition->size; offset += CHUNK) {
    const size_t remaining = static_cast<size_t>(partition->size) - offset;
    const size_t length = remaining < CHUNK ? remaining : CHUNK;
    if (esp_partition_read(partition, offset, buffer, length) != ESP_OK)
      return false;
    for (size_t index = 0; index < length; index++) {
      if (buffer[index] != 0xFF)
        return false;
    }
  }
  return true;
}

void TsdbComponent::add_column(const char *name, double scale, double offset) {
  TsdbColumn column;
  column.name = StringRef(name);
  column.scale = scale;
  column.offset = offset;
  this->columns_.push_back(column);
}

void TsdbComponent::set_column_source(size_t index, sensor::Sensor *source) { this->columns_[index].source = source; }

void TsdbComponent::setup() {
  if (this->columns_.empty()) {
    ESP_LOGE(TAG, "tsdb: no columns configured");
    this->mark_failed(LOG_STR("no columns configured"));
    return;
  }
  // Size the write buffer once so write_record_() never allocates.
  this->values_.reserve(this->columns_.size());
  if (!this->mount_filesystem_()) {
    this->mark_failed(LOG_STR("mounting the LittleFS partition"));
    return;
  }
  if (!this->open_database_()) {
    this->unmount_filesystem_();  // the failed open must not leak the mount
    this->mark_failed(LOG_STR("opening the database"));
    return;
  }
  ESP_LOGI(TAG, "%s: history opened", this->file_.c_str());
}

void TsdbComponent::on_shutdown() {
  if (this->db_ == nullptr) {
    this->unmount_filesystem_();
    return;
  }
  this->flush_(true);
  ESP_LOGI(TAG, "%s: closing on shutdown", this->file_.c_str());
  this->close_database_();
  this->unmount_filesystem_();
}

bool TsdbComponent::mount_filesystem_() {
  const esp_partition_t *partition = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, static_cast<esp_partition_subtype_t>(LITTLEFS_SUBTYPE), this->partition_label_.c_str());
  if (partition == nullptr) {
    ESP_LOGE(TAG, "%s: no data/littlefs partition '%s' - flash the partition table over USB", this->file_.c_str(),
             this->partition_label_.c_str());
    return false;
  }
  // A stale, smaller partition would make max_file_size wrong (later ENOSPC).
  if (partition->size < this->partition_size_) {
    ESP_LOGE(TAG, "%s: partition '%s' is %" PRIu32 " bytes but %" PRIu32 " configured - flash the partition table",
             this->file_.c_str(), this->partition_label_.c_str(), static_cast<uint32_t>(partition->size),
             this->partition_size_);
    return false;
  }

  esp_vfs_littlefs_conf_t conf = {};
  conf.base_path = this->mount_point_.c_str();
  conf.partition_label = this->partition_label_.c_str();
  conf.partition = partition;
  conf.format_if_mount_failed = false;
  conf.dont_mount = false;
  conf.grow_on_mount = false;

  esp_err_t err = esp_vfs_littlefs_register(&conf);
  if (err != ESP_OK && this->format_on_first_boot_ && partition_is_blank(partition)) {
    ESP_LOGW(TAG, "%s: partition '%s' is unformatted - formatting it", this->file_.c_str(),
             this->partition_label_.c_str());
    err = esp_littlefs_format(this->partition_label_.c_str());
    if (err == ESP_OK) {
      err = esp_vfs_littlefs_register(&conf);
    } else {
      ESP_LOGE(TAG, "%s: formatting '%s' failed: %s", this->file_.c_str(), this->partition_label_.c_str(),
               esp_err_to_name(err));
    }
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "%s: mounting '%s' on %s failed: %s", this->file_.c_str(), this->partition_label_.c_str(),
             this->mount_point_.c_str(), esp_err_to_name(err));
    return false;
  }

  this->mounted_ = true;
  size_t total = 0;
  size_t used = 0;
  if (esp_littlefs_info(this->partition_label_.c_str(), &total, &used) == ESP_OK) {
    ESP_LOGI(TAG, "%s: %s mounted on %s (%" PRIu32 " of %" PRIu32 " bytes used)", this->file_.c_str(),
             this->partition_label_.c_str(), this->mount_point_.c_str(), static_cast<uint32_t>(used),
             static_cast<uint32_t>(total));
  }
  return true;
}

// A failed or shut-down database must not leave the VFS mount behind.
void TsdbComponent::unmount_filesystem_() {
  if (!this->mounted_)
    return;
  const esp_err_t err = esp_vfs_littlefs_unregister(this->partition_label_.c_str());
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "%s: unmounting the filesystem failed: %s", this->file_.c_str(), esp_err_to_name(err));
  }
  this->mounted_ = false;
}

bool TsdbComponent::open_database_() {
  std::vector<const char *> names;
  names.reserve(this->columns_.size());
  for (const TsdbColumn &column : this->columns_)
    names.push_back(column.name.c_str());

  const uint32_t max_records = tsdbmath::capacity_for_bytes(this->max_file_size_, names.size(), this->index_stride_);
  if (max_records == 0) {
    ESP_LOGE(TAG, "%s: max_file_size too small for %" PRIu32 " columns", this->file_.c_str(),
             static_cast<uint32_t>(names.size()));
    return false;
  }
  const std::string path = this->mount_point_ + "/" + this->file_;

  tsdb_config_t config = {};
  config.filepath = path.c_str();
  config.num_params = static_cast<uint8_t>(names.size());
  config.param_names = names.data();
  config.max_records = max_records;
  config.index_stride = this->index_stride_;
  config.buffer_pool_size = this->buffer_pool_size_;
  config.alloc_strategy = TSDB_ALLOC_AUTO;
  config.use_paged_allocation = false;
  config.page_size = 2048;

  this->db_ = tsdb_open(&config);
  if (this->db_ == nullptr) {
    ESP_LOGE(TAG, "%s: esp_tsdb refused to open %s (%" PRIu32 " columns, up to %" PRIu32 " records)",
             this->file_.c_str(), path.c_str(), static_cast<uint32_t>(names.size()), max_records);
    return false;
  }

  // An existing file keeps its capacity and index stride it was created with.
  tsdb_stats_t stats = {};
  const bool have_stats = tsdb_get_stats_h(static_cast<tsdb_t *>(this->db_), &stats) == ESP_OK;
  if (!have_stats) {
    ESP_LOGW(TAG, "%s: reading the stored statistics failed - the timestamp guard starts empty", this->file_.c_str());
  }
  uint32_t saved_stride = 0;
  const bool have_stride = this->read_saved_index_stride_(&saved_stride) && saved_stride > 0;
  const uint32_t records = have_stats && stats.max_records > 0 ? stats.max_records : max_records;
  const uint32_t stride = have_stride ? saved_stride : this->index_stride_;
  const uint64_t bytes = tsdbmath::file_bytes_for(records, names.size(), stride);
  ESP_LOGI(TAG, "%s: %" PRIu32 " columns, %" PRIu32 " records (up to %" PRIu32 " KB) at %s", this->file_.c_str(),
           static_cast<uint32_t>(names.size()), records, static_cast<uint32_t>(bytes / 1024), path.c_str());
  if (have_stats) {
    // Seed the monotonic timestamp guard with the newest record already stored.
    this->last_timestamp_ = stats.newest_timestamp;
    this->uptime_offset_ = stats.newest_timestamp + 1;
    if (records != max_records) {
      ESP_LOGW(TAG,
               "%s: saved capacity is %" PRIu32 " records, the configured budget asks for %" PRIu32
               " - recreate the history to change it",
               this->file_.c_str(), records, max_records);
    }
  }
  if (have_stride && saved_stride != this->index_stride_) {
    ESP_LOGW(TAG,
             "%s: saved index stride is %" PRIu32 ", the configured value is %" PRIu32
             " - recreate the history to change it",
             this->file_.c_str(), saved_stride, this->index_stride_);
  }
  return true;
}

// The engine has no getter for a saved file's index stride; read it from the header.
bool TsdbComponent::read_saved_index_stride_(uint32_t *stride) const {
  const std::string path = this->mount_point_ + "/" + this->file_;
  FILE *file = fopen(path.c_str(), "rb");
  if (file == nullptr)
    return false;
  const int32_t field = static_cast<int32_t>(offsetof(tsdb_header_t, index_stride));
  uint32_t value = 0;
  const bool ok = fseek(file, field, SEEK_SET) == 0 && fread(&value, sizeof(value), 1, file) == 1;
  fclose(file);
  if (!ok)
    return false;
  *stride = value;
  return true;
}

void TsdbComponent::close_database_() {
  if (this->db_ == nullptr)
    return;
  const esp_err_t err = tsdb_close_h(static_cast<tsdb_t *>(this->db_));
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "%s: closing the database failed: %s", this->file_.c_str(), esp_err_to_name(err));
  }
  this->db_ = nullptr;
}

// tsdb_sync_h() closes the handle when its fclose/reopen fails and every later
// write returns ESP_ERR_INVALID_STATE, so drop it and reopen on a later interval.
void TsdbComponent::invalidate_database_(const LogString *reason) {
  if (this->db_ == nullptr)
    return;
  ESP_LOGE(TAG, "%s: database unusable (%s) - will reopen on a later interval", this->file_.c_str(),
           LOG_STR_ARG(reason));
  this->close_database_();
  this->db_lost_ = true;
  this->status_set_warning(LOG_STR("history database unavailable"));
}

void TsdbComponent::reopen_if_lost_() {
  if (!this->db_lost_)
    return;
  // A failed open logs an error, so retry at most every REOPEN_RETRY_MS.
  const uint32_t now = App.get_loop_component_start_time();
  if (this->last_reopen_ != 0 && now - this->last_reopen_ < REOPEN_RETRY_MS)
    return;
  this->last_reopen_ = now == 0 ? 1 : now;
  if (!this->open_database_()) {
    // Count the failed retry so a database that never recovers is visible.
    this->write_errors_++;
    return;
  }
  this->db_lost_ = false;
  ESP_LOGI(TAG, "%s: history reopened", this->file_.c_str());
  this->status_clear_warning();
}

void TsdbComponent::update() {
  if (this->db_ == nullptr) {
    // Count the skipped interval, so an outage is visible instead of a flat count.
    if (this->db_lost_)
      this->dropped_++;
    this->reopen_if_lost_();
    return;
  }
  this->write_record_();
  this->flush_(false);
}

bool TsdbComponent::write_record_() {
  // Any synced time platform also sets the system clock, so ::time() is a usable fallback.
  uint32_t timestamp = static_cast<uint32_t>(::time(nullptr));
  bool have_time = tsdbmath::is_valid_timestamp(timestamp);
#ifdef USE_TIME
  if (this->time_ != nullptr) {
    const ESPTime now = this->time_->now();
    if (now.is_valid()) {
      timestamp = static_cast<uint32_t>(now.timestamp);
      have_time = true;
    }
  }
#endif

  if (!have_time) {
    if (this->require_time_) {
      if (!this->warned_time_) {
        this->warned_time_ = true;
        ESP_LOGW(TAG, "%s: no valid time yet - no record written", this->file_.c_str());
      }
      // Count the skipped interval so a clock that never syncs is visible.
      this->dropped_++;
      return false;
    }
    // Without a clock the value counts seconds since boot and restarts at 0 on every
    // reboot; the offset continues the series after the newest stored record. It is
    // added only here, so it can never make an unsynced clock look like a real time.
    timestamp += this->uptime_offset_;
    if (!this->warned_time_) {
      this->warned_time_ = true;
      ESP_LOGW(TAG, "%s: no valid time yet - storing an uptime timestamp", this->file_.c_str());
    }
  } else {
    this->warned_time_ = false;
  }

  // Records are ordered by timestamp, so refuse one that sorts before the newest
  // stored: an NTP correction moved back, or uptime restarting at 0 after a reboot.
  if (this->last_timestamp_ != 0 && timestamp < this->last_timestamp_) {
    if (!this->warned_backwards_) {
      this->warned_backwards_ = true;
      ESP_LOGW(TAG, "%s: time is earlier than the last stored record - no record written", this->file_.c_str());
    }
    this->dropped_++;
    return false;
  }

  this->values_.clear();
  for (TsdbColumn &column : this->columns_) {
    const bool has_value =
        column.source != nullptr && column.source->has_state() && !tsdbmath::is_missing(column.source->state);
    if (has_value) {
      column.hold_warned = false;  // so a later outage logs again
      bool saturated = false;
      column.held = tsdbmath::encode(column.source->state, column.scale, column.offset, &saturated);
      if (saturated) {
        if (this->saturated_++ % SATURATION_LOG_EVERY == 0) {
          ESP_LOGW(TAG, "%s: %s out of range for scale_factor %.6g - clamped, raise it?", this->file_.c_str(),
                   column.name.c_str(), column.scale);
        }
      }
      this->values_.push_back(column.held);
      continue;
    }
    if (this->missing_policy_ == tsdbmath::MISSING_HOLD && column.held != tsdbmath::RAW_UNKNOWN) {
      // A held value is a stale one: count it so data loss stays visible.
      this->held_++;
      if (!column.hold_warned) {
        column.hold_warned = true;
        ESP_LOGW(TAG, "%s: %s has no value - holding the last value", this->file_.c_str(), column.name.c_str());
      }
      this->values_.push_back(column.held);
      continue;
    }
    if (this->missing_policy_ == tsdbmath::MISSING_SENTINEL) {
      this->values_.push_back(tsdbmath::RAW_UNKNOWN);
      continue;
    }
    this->dropped_++;
    // Rate limited on its own count: dropped_ also counts unsynced-clock skips.
    this->dropped_missing_++;
    if (this->dropped_missing_ == 1 || this->dropped_missing_ % DROP_LOG_EVERY == 0) {
      ESP_LOGW(TAG, "%s: %s has no value - record dropped", this->file_.c_str(), column.name.c_str());
    } else {
      ESP_LOGD(TAG, "%s: %s has no value - record dropped", this->file_.c_str(), column.name.c_str());
    }
    return false;
  }

  const esp_err_t err = tsdb_write_h(static_cast<tsdb_t *>(this->db_), timestamp, this->values_.data());
  if (err != ESP_OK) {
    this->write_errors_++;
    ESP_LOGE(TAG, "%s: writing a record failed: %s", this->file_.c_str(), esp_err_to_name(err));
    // The engine closed the handle; recover instead of failing every write.
    if (!tsdb_is_initialized_h(static_cast<tsdb_t *>(this->db_)))
      this->invalidate_database_(LOG_STR("a write failed and closed the database"));
    return false;
  }
  this->writes_++;
  this->last_timestamp_ = timestamp;
  this->warned_backwards_ = false;
  this->dirty_ = true;
  return true;
}

void TsdbComponent::flush_(bool force) {
  if (this->db_ == nullptr)
    return;
  const uint32_t now = App.get_loop_component_start_time();
  if (!force && !this->dirty_)
    return;
  if (!force && this->last_sync_ != 0 && this->sync_interval_ > 0 && now - this->last_sync_ < this->sync_interval_)
    return;
  this->last_sync_ = now == 0 ? 1 : now;

  // tsdb_sync_h() folds the in-file header back into the database; littlefs
  // copy-on-writes from there to end of file (3.7-5.1 s for a 192-268 KB file),
  // so it blocks the main loop for seconds, and esp_tsdb 2.4.3 has no other way
  // to refresh the header. Records are already on disk (each write fsyncs its
  // block and a header sidecar), so sync_interval is the knob: a longer interval
  // stalls less often, at the cost of a staler header after an unclean shutdown.
  const esp_err_t err = tsdb_sync_h(static_cast<tsdb_t *>(this->db_));
  if (err != ESP_OK) {
    this->write_errors_++;
    ESP_LOGE(TAG, "%s: sync failed: %s", this->file_.c_str(), esp_err_to_name(err));
    // A failed sync closes the handle; reopen rather than write into a dead database.
    if (!tsdb_is_initialized_h(static_cast<tsdb_t *>(this->db_)))
      this->invalidate_database_(LOG_STR("a sync failed and closed the database"));
    return;
  }
  this->dirty_ = false;
  ESP_LOGD(TAG, "%s: synced after %" PRIu32 " writes", this->file_.c_str(), this->writes_);
}

void TsdbComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "tsdb:");
  ESP_LOGCONFIG(TAG, "  file: %s/%s", this->mount_point_.c_str(), this->file_.c_str());
  ESP_LOGCONFIG(TAG, "  partition: %s, %" PRIu32 " KB", this->partition_label_.c_str(), this->partition_size_ / 1024);
  ESP_LOGCONFIG(TAG, "  columns: %" PRIu32, static_cast<uint32_t>(this->columns_.size()));
  for (const TsdbColumn &column : this->columns_) {
    ESP_LOGCONFIG(TAG, "    %s: scale %.6g, offset %.6g", column.name.c_str(), column.scale, column.offset);
  }
  ESP_LOGCONFIG(TAG, "  write interval: %" PRIu32 " ms, sync interval: %" PRIu32 " ms", this->get_update_interval(),
                this->sync_interval_);
  ESP_LOGCONFIG(
      TAG, "  writes: %" PRIu32 ", errors: %" PRIu32 ", dropped: %" PRIu32 ", clamped: %" PRIu32 ", held: %" PRIu32,
      this->writes_, this->write_errors_, this->dropped_, this->saturated_, this->held_);
}

}  // namespace esphome::tsdb
