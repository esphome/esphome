#include "tsdb.h"

#include <array>
#include <cinttypes>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <ctime>
#include <string>
#include <sys/stat.h>

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_littlefs.h"
#include "esp_partition.h"
#include "esp_tsdb.h"

namespace esphome::tsdb {

ESPHOME_LOG_TAG(TAG, "tsdb");

// ESP_PARTITION_SUBTYPE_DATA_LITTLEFS (0x83), missing from some IDF 5.x headers.
static constexpr uint8_t LITTLEFS_SUBTYPE = 0x83;
// Diagnostics refresh period.
static constexpr uint32_t STATS_INTERVAL_MS = 60000;
// Every Nth dropped record logs at WARN, the rest at DEBUG.
static constexpr uint32_t DROP_LOG_EVERY = 10;
// Rate limit for the free-space and out-of-range warnings.
static constexpr uint32_t PROBE_LOG_EVERY = 100;
static constexpr uint32_t SATURATION_LOG_EVERY = 100;
// One CSV dump line.
static constexpr size_t CSV_LINE_SIZE = 256;
// Dump window margin for jitter and single gaps.
static constexpr uint32_t DUMP_WINDOW_MARGIN = 2;

// dump_config() names, indexed by the tsdbmath enums (last entry is the fallback).
PROGMEM_STRING_TABLE(MissingPolicyStrings, "skip", "hold", "sentinel", "unknown");
PROGMEM_STRING_TABLE(MemoryModeStrings, "internal", "psram", "auto", "unknown");

static const LogString *missing_policy_to_string(tsdbmath::MissingPolicy policy) {
  return MissingPolicyStrings::get_log_str(static_cast<uint8_t>(policy), MissingPolicyStrings::LAST_INDEX);
}

static const LogString *memory_mode_to_string(tsdbmath::MemoryMode mode) {
  return MemoryModeStrings::get_log_str(static_cast<uint8_t>(mode), MemoryModeStrings::LAST_INDEX);
}

// esp_tsdb free-space callback takes no context; the mounted label lives here.
static const char *g_free_space_label = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
// Failed free-space probes, to rate limit the warning the callback emits.
static uint32_t g_probe_failures = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static_assert(tsdbmath::BLOCK_SIZE == TSDB_BLOCK_SIZE, "esp_tsdb changed TSDB_BLOCK_SIZE");
static_assert(tsdbmath::BLOCK_HEADER_SIZE == TSDB_BLOCK_HEADER_SIZE, "esp_tsdb changed the block header");
static_assert(tsdbmath::records_per_block(4) == (TSDB_BLOCK_SIZE - TSDB_BLOCK_HEADER_SIZE) / (4 + 4 * 2),
              "esp_tsdb changed the record geometry");
static_assert(tsdbmath::max_records_for_bytes(3072, 4) == TSDB_CALC_MAX_RECORDS(3072, 4),
              "esp_tsdb changed TSDB_CALC_MAX_RECORDS()");
static_assert(sizeof(tsdb_header_t) == tsdbmath::HEADER_BYTES, "esp_tsdb changed sizeof(tsdb_header_t)");
static_assert(tsdbmath::FILE_MAGIC == static_cast<uint32_t>(TSDB_MAGIC), "esp_tsdb changed TSDB_MAGIC");
static_assert(tsdbmath::MAX_COLUMNS == static_cast<uint32_t>(TSDB_MAX_PARAMS), "esp_tsdb changed TSDB_MAX_PARAMS");
static_assert(offsetof(tsdb_header_t, magic) == tsdbmath::HEADER_MAGIC_OFFSET, "esp_tsdb moved magic");
static_assert(offsetof(tsdb_header_t, num_params) == tsdbmath::HEADER_COLUMNS_OFFSET, "esp_tsdb moved num_params");
static_assert(tsdbmath::capacity_for_bytes(384 * 1024, 4) <= TSDB_CALC_MAX_RECORDS(384 * 1024, 4),
              "capacity solver may never exceed the engine formula");
static_assert(static_cast<int>(tsdbmath::MEMORY_INTERNAL) == static_cast<int>(TSDB_ALLOC_INTERNAL_RAM),
              "esp_tsdb changed tsdb_alloc_strategy_t");
static_assert(static_cast<int>(tsdbmath::MEMORY_PSRAM) == static_cast<int>(TSDB_ALLOC_PSRAM),
              "esp_tsdb changed tsdb_alloc_strategy_t");
static_assert(static_cast<int>(tsdbmath::MEMORY_AUTO) == static_cast<int>(TSDB_ALLOC_AUTO),
              "esp_tsdb changed tsdb_alloc_strategy_t");

// True when every byte of the partition is 0xFF (scanned in 256-byte chunks).
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

void TsdbComponent::set_column_average_sensor(size_t index, sensor::Sensor *sensor) {
  this->columns_[index].average_sensor = sensor;
}

void TsdbComponent::set_column_min_sensor(size_t index, sensor::Sensor *sensor) {
  this->columns_[index].min_sensor = sensor;
}

void TsdbComponent::set_column_max_sensor(size_t index, sensor::Sensor *sensor) {
  this->columns_[index].max_sensor = sensor;
}

void TsdbComponent::setup() {
  if (this->columns_.empty()) {
    ESP_LOGE(TAG, "tsdb: no columns configured");
    this->log_init_failure_();
    this->fail_setup_(LOG_STR("no columns configured"));
    return;
  }
  if (!this->mount_filesystem_()) {
    this->log_init_failure_();
    this->fail_setup_(LOG_STR("mounting the LittleFS partition"));
    return;
  }
  if (!this->open_database_()) {
    this->log_init_failure_();
    this->unmount_filesystem_();  // the failed open must not leak the mount
    this->fail_setup_(LOG_STR("opening the database"));
    return;
  }
  this->publish_log_("history opened");
  // Report the diagnostics now instead of leaving them unknown until the first
  // stats tick.
  this->last_stats_ = millis();
  this->publish_stats_();
}

// Count the failed initialisation so the diagnostics sensor reports it instead
// of staying "unknown" on a device whose database never opened.
void TsdbComponent::fail_setup_(const LogString *reason) {
  this->write_errors_++;
  this->publish_errors_();
  this->mark_failed(reason);
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
    this->mount_error_ = ESP_ERR_NOT_FOUND;
    ESP_LOGE(TAG, "%s: no data/littlefs partition '%s' - flash the partition table over USB", this->file_.c_str(),
             this->partition_label_.c_str());
    return false;
  }
  // A partition_size change needs the table reflashed; a stale, smaller one
  // would make max_file_size wrong and surface later as ENOSPC write errors.
  if (partition->size < this->partition_size_) {
    this->mount_error_ = ESP_ERR_INVALID_SIZE;
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
    this->format_attempted_ = true;
    this->format_error_ = esp_littlefs_format(this->partition_label_.c_str());
    if (this->format_error_ == ESP_OK) {
      err = esp_vfs_littlefs_register(&conf);
    } else {
      ESP_LOGE(TAG, "%s: formatting '%s' failed: %s", this->file_.c_str(), this->partition_label_.c_str(),
               esp_err_to_name(this->format_error_));
    }
  }
  this->mount_error_ = err;  // the final result, for log_init_failure_()
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "%s: mounting '%s' on %s failed: %s", this->file_.c_str(), this->partition_label_.c_str(),
             this->mount_point_.c_str(), esp_err_to_name(err));
    return false;
  }

  this->mounted_ = true;
  g_free_space_label = this->partition_label_.c_str();
  size_t total = 0;
  size_t used = 0;
  if (esp_littlefs_info(this->partition_label_.c_str(), &total, &used) == ESP_OK) {
    ESP_LOGI(TAG, "%s: %s mounted on %s (%" PRIu32 " of %" PRIu32 " bytes used)", this->file_.c_str(),
             this->partition_label_.c_str(), this->mount_point_.c_str(), static_cast<uint32_t>(used),
             static_cast<uint32_t>(total));
  }
  return true;
}

static bool database_file_exists(const std::string &path) {
  FILE *file = fopen(path.c_str(), "rb");
  if (file == nullptr)
    return false;
  fclose(file);
  return true;
}

// Deletes the database and its .h0/.h1 header sidecars.
static void remove_database_files(const std::string &path) {
  static const char *const SUFFIXES[] = {"", ".h0", ".h1"};
  for (const char *suffix : SUFFIXES) {
    const std::string file = path + suffix;
    if (remove(file.c_str()) != 0) {
      ESP_LOGW(TAG, "%s: could not be deleted", file.c_str());
    }
  }
}

// Stored column count, or 0 when absent/unreadable (never a delete reason).
static uint32_t read_stored_columns(const std::string &path) {
  FILE *file = fopen(path.c_str(), "rb");
  if (file == nullptr)
    return 0;
  uint8_t header[tsdbmath::HEADER_BYTES] = {};
  const size_t read = fread(header, 1, sizeof(header), file);
  fclose(file);
  return tsdbmath::stored_columns(header, read);
}

// One stat() covers both the presence and the size of the database file.
void TsdbComponent::log_init_failure_() {
  const std::string path = this->mount_point_ + "/" + this->file_;
  const esp_partition_t *partition = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, static_cast<esp_partition_subtype_t>(LITTLEFS_SUBTYPE), this->partition_label_.c_str());
  if (partition == nullptr) {
    ESP_LOGE(TAG, "  partition '%s': NOT FOUND", this->partition_label_.c_str());
  } else {
    ESP_LOGE(TAG, "  partition '%s': at 0x%08" PRIx32 ", %" PRIu32 " bytes", partition->label,
             static_cast<uint32_t>(partition->address), static_cast<uint32_t>(partition->size));
  }
  struct stat info = {};
  const bool present = ::stat(path.c_str(), &info) == 0;
  ESP_LOGE(TAG, "  mount: %s, %s: %s", esp_err_to_name(this->mount_error_), path.c_str(),
           present ? LOG_STR_LITERAL("present") : LOG_STR_LITERAL("missing"));
  if (present) {
    ESP_LOGE(TAG, "    %" PRIu32 " bytes", static_cast<uint32_t>(info.st_size));
  }
  if (this->format_attempted_) {
    ESP_LOGE(TAG, "  format attempt: %s", esp_err_to_name(this->format_error_));
  }
  ESP_LOGE(TAG, "  heap: %" PRIu32 " bytes internal free",
           static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL)));
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
  g_free_space_label = nullptr;
}

bool TsdbComponent::open_database_() {
  const uint32_t columns = static_cast<uint32_t>(this->columns_.size());
  std::array<const char *, TSDB_COLUMN_COUNT> names{};
  for (uint32_t index = 0; index < columns; index++)
    names[index] = this->columns_[index].name.c_str();

  const uint32_t max_records = tsdbmath::capacity_for_bytes(this->max_file_size_, columns, this->index_stride_);
  if (max_records == 0) {
    ESP_LOGE(TAG, "%s: max_file_size too small for %" PRIu32 " columns", this->file_.c_str(), columns);
    return false;
  }
  const std::string path = this->mount_point_ + "/" + this->file_;

  tsdb_config_t config = {};
  config.filepath = path.c_str();
  config.num_params = static_cast<uint8_t>(columns);
  config.param_names = names.data();
  config.max_records = max_records;
  config.index_stride = this->index_stride_;
  config.buffer_pool_size = this->buffer_pool_size_;
  config.alloc_strategy = static_cast<tsdb_alloc_strategy_t>(this->memory_mode_);
  config.use_paged_allocation = this->paged_allocation_;
  config.page_size = this->page_size_;
  config.min_free_bytes = this->min_free_bytes_;
  config.free_space_cb = this->min_free_bytes_ > 0 ? free_space_probe : nullptr;

  this->db_ = tsdb_open(&config);
  if (this->db_ == nullptr && this->recreate_on_schema_change_) {
    // A column-count change makes the engine refuse the file; only the file's
    // own header may authorize deleting it (other failures keep the file).
    const uint32_t stored = read_stored_columns(path);
    const uint32_t configured = columns;
    if (stored != 0 && stored != configured) {
      ESP_LOGW(TAG, "%s: schema changed (stored %" PRIu32 ", configured %" PRIu32 ") - recreating", this->file_.c_str(),
               stored, configured);
      remove_database_files(path);
      this->db_ = tsdb_open(&config);
      if (this->db_ != nullptr)
        this->publish_log_("history recreated (the column set changed)");
    } else if (database_file_exists(path)) {
      ESP_LOGW(TAG, "%s: opening failed but the stored schema is not a mismatch - keeping the file",
               this->file_.c_str());
    }
  }
  if (this->db_ == nullptr) {
    ESP_LOGE(TAG, "%s: esp_tsdb refused to open %s (%" PRIu32 " columns, up to %" PRIu32 " records)",
             this->file_.c_str(), path.c_str(), columns, max_records);
    return false;
  }

  const uint64_t bytes = tsdbmath::file_bytes_for(max_records, columns, this->index_stride_);
  ESP_LOGI(TAG, "%s: %" PRIu32 " columns, %" PRIu32 " records (up to %" PRIu32 " KB) at %s", this->file_.c_str(),
           columns, max_records, static_cast<uint32_t>(bytes / 1024), path.c_str());
  return true;
}

void TsdbComponent::close_database_() {
  if (this->db_ == nullptr)
    return;
  // The final header and index write happens here, so a failure means a
  // possibly inconsistent file on the next boot.
  const esp_err_t err = tsdb_close_h(static_cast<tsdb_t *>(this->db_));
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "%s: closing the database failed: %s", this->file_.c_str(), esp_err_to_name(err));
  }
  this->db_ = nullptr;
}

uint64_t TsdbComponent::free_space_probe() {
  size_t total = 0;
  size_t used = 0;
  if (g_free_space_label == nullptr || esp_littlefs_info(g_free_space_label, &total, &used) != ESP_OK) {
    // Fail closed: an unreadable free-space value must not disable min_free_bytes.
    if (g_probe_failures++ % PROBE_LOG_EVERY == 0) {
      ESP_LOGW(TAG, "free space could not be read - refusing to grow the database");
    }
    return 0;
  }
  return static_cast<uint64_t>(total) - static_cast<uint64_t>(used);
}

void TsdbComponent::update() {
  if (this->db_ == nullptr)
    return;
  this->handle_requests_();
  this->write_record_();
  this->flush_(false);

  const uint32_t now = App.get_loop_component_start_time();
  if (this->aggregate_interval_ > 0 &&
      (this->last_aggregate_ == 0 || now - this->last_aggregate_ >= this->aggregate_interval_)) {
    this->last_aggregate_ = now;
    this->publish_aggregates_();
  }
  if (this->last_stats_ == 0 || now - this->last_stats_ >= STATS_INTERVAL_MS) {
    this->last_stats_ = now;
    this->publish_stats_();
  }
}

void TsdbComponent::handle_requests_() {
  auto *db = static_cast<tsdb_t *>(this->db_);

  if (this->clear_requested_) {
    this->clear_requested_ = false;
    const esp_err_t err = tsdb_clear_h(db);
    if (err != ESP_OK) {
      this->write_errors_++;
      this->publish_errors_();
      ESP_LOGE(TAG, "%s: clearing the history failed: %s", this->file_.c_str(), esp_err_to_name(err));
    } else {
      this->records_ = 0;
      this->writes_ = 0;
      this->dirty_ = false;
      ESP_LOGW(TAG, "%s: history cleared on request", this->file_.c_str());
      this->publish_log_("history cleared");
    }
  }

  if (this->flush_requested_) {
    this->flush_requested_ = false;
    this->flush_(true);
    ESP_LOGI(TAG, "%s: flushed on request", this->file_.c_str());
    this->publish_log_("history flushed");
  }

  if (this->dump_requested_ > 0) {
    const uint32_t rows = this->dump_requested_;
    this->dump_requested_ = 0;
    this->dump_csv_(rows);
  }
}

bool TsdbComponent::write_record_() {
  // Any synced time platform also sets the system clock, so ::time() is a usable fallback.
  uint32_t timestamp = static_cast<uint32_t>(::time(nullptr));
  if (this->time_ != nullptr) {
    const ESPTime now = this->time_->now();
    if (now.is_valid())
      timestamp = static_cast<uint32_t>(now.timestamp);
  }

  if (!tsdbmath::is_valid_timestamp(timestamp)) {
    if (this->require_time_) {
      if (!this->warned_time_) {
        this->warned_time_ = true;
        ESP_LOGW(TAG, "%s: no valid time yet - no record written", this->file_.c_str());
      }
      // Count the skipped interval so a clock that never syncs is visible.
      this->dropped_++;
      this->publish_errors_();
      return false;
    }
    if (!this->warned_time_) {
      this->warned_time_ = true;
      ESP_LOGW(TAG, "%s: no valid time yet - storing an uptime timestamp", this->file_.c_str());
    }
  } else {
    this->warned_time_ = false;
  }

  this->values_.clear();
  for (TsdbColumn &column : this->columns_) {
    const bool has_value =
        column.source != nullptr && column.source->has_state() && !tsdbmath::is_missing(column.source->state);
    if (has_value) {
      bool saturated = false;
      column.held = tsdbmath::encode(column.source->state, column.scale, column.offset, &saturated);
      if (saturated) {
        if (this->saturated_++ % SATURATION_LOG_EVERY == 0) {
          ESP_LOGW(TAG, "%s: %s out of range for scale_factor %.6g - clamped, raise it?", this->file_.c_str(),
                   column.name.c_str(), column.scale);
        }
        this->publish_errors_();  // a clamped value is one of the anomalies reported
      }
      this->values_.push_back(column.held);
      continue;
    }
    if (this->missing_policy_ == tsdbmath::MISSING_HOLD && column.held != tsdbmath::RAW_UNKNOWN) {
      this->values_.push_back(column.held);
      continue;
    }
    if (this->missing_policy_ == tsdbmath::MISSING_SENTINEL) {
      this->values_.push_back(tsdbmath::RAW_UNKNOWN);
      continue;
    }
    this->dropped_++;
    if (this->dropped_ == 1 || this->dropped_ % DROP_LOG_EVERY == 0) {
      ESP_LOGW(TAG, "%s: %s has no value - record dropped", this->file_.c_str(), column.name.c_str());
    } else {
      ESP_LOGD(TAG, "%s: %s has no value - record dropped", this->file_.c_str(), column.name.c_str());
    }
    this->publish_errors_();
    return false;
  }

  const esp_err_t err = tsdb_write_h(static_cast<tsdb_t *>(this->db_), timestamp, this->values_.data());
  if (err != ESP_OK) {
    this->write_errors_++;
    ESP_LOGE(TAG, "%s: writing a record failed: %s", this->file_.c_str(), esp_err_to_name(err));
    this->publish_errors_();
    return false;
  }
  this->writes_++;
  this->dirty_ = true;
  return true;
}

// The diagnostics sensor counts every anomaly - failed writes/syncs, dropped
// records and clamped values - so data loss or corruption is visible.
void TsdbComponent::publish_errors_() {
  if (this->errors_sensor_ == nullptr)
    return;
  this->errors_sensor_->publish_state(this->write_errors_ + this->dropped_ + this->saturated_);
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

  const esp_err_t err = tsdb_sync_h(static_cast<tsdb_t *>(this->db_));
  if (err != ESP_OK) {
    this->write_errors_++;
    ESP_LOGE(TAG, "%s: sync failed: %s", this->file_.c_str(), esp_err_to_name(err));
    this->publish_errors_();
    return;
  }
  this->dirty_ = false;
  ESP_LOGD(TAG, "%s: synced after %" PRIu32 " writes", this->file_.c_str(), this->writes_);
}

void TsdbComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "tsdb:");
  ESP_LOGCONFIG(TAG, "  file: %s/%s", this->mount_point_.c_str(), this->file_.c_str());
  ESP_LOGCONFIG(TAG, "  partition: %s, %" PRIu32 " KB, format on first boot: %s, recreate on schema change: %s",
                this->partition_label_.c_str(), this->partition_size_ / 1024, YESNO(this->format_on_first_boot_),
                YESNO(this->recreate_on_schema_change_));
  ESP_LOGCONFIG(TAG, "  budget: max file %" PRIu32 " KB, min free %" PRIu32 " KB, index stride %" PRIu32,
                this->max_file_size_ / 1024, this->min_free_bytes_ / 1024, this->index_stride_);
  ESP_LOGCONFIG(TAG, "  columns: %" PRIu32, static_cast<uint32_t>(this->columns_.size()));
  for (const TsdbColumn &column : this->columns_) {
    ESP_LOGCONFIG(TAG, "    %s: scale %.6g, offset %.6g", column.name.c_str(), column.scale, column.offset);
  }
  ESP_LOGCONFIG(TAG, "  write interval: %" PRIu32 " ms, sync interval: %" PRIu32 " ms", this->get_update_interval(),
                this->sync_interval_);
  ESP_LOGCONFIG(TAG, "  buffer pool: %" PRIu32 " bytes (page size: %" PRIu32 "), memory: %s, paged allocation: %s",
                this->buffer_pool_size_, this->page_size_, LOG_STR_ARG(memory_mode_to_string(this->memory_mode_)),
                YESNO(this->paged_allocation_));
  ESP_LOGCONFIG(TAG, "  time required: %s, missing values: %s", YESNO(this->require_time_),
                LOG_STR_ARG(missing_policy_to_string(this->missing_policy_)));
  ESP_LOGCONFIG(TAG, "  aggregates: window %" PRIu32 " s, interval %" PRIu32 " ms, dump rows: %" PRIu32,
                this->aggregate_window_, this->aggregate_interval_, this->dump_rows_);
  ESP_LOGCONFIG(TAG, "  writes: %" PRIu32 ", errors: %" PRIu32 ", dropped: %" PRIu32 ", clamped: %" PRIu32,
                this->writes_, this->write_errors_, this->dropped_, this->saturated_);
}

void TsdbComponent::publish_aggregates_() {
  if (this->time_ == nullptr || this->aggregate_window_ == 0)
    return;
  const ESPTime now = this->time_->now();
  if (!now.is_valid())
    return;
  const uint32_t end = static_cast<uint32_t>(now.timestamp);
  const uint32_t start = end > this->aggregate_window_ ? end - this->aggregate_window_ : 1;

  auto *db = static_cast<tsdb_t *>(this->db_);
  uint32_t count = 0;
  const bool has_records = tsdb_query_count_h(db, start, end, &count) == ESP_OK && count > 0;

  auto publish = [&](sensor::Sensor *target, uint8_t param, tsdb_agg_type_t aggregation, const TsdbColumn &column) {
    if (!has_records) {
      target->publish_state(NAN);
      return;
    }
    int32_t result = 0;
    if (tsdb_aggregate_h(db, start, end, param, aggregation, &result) != ESP_OK) {
      target->publish_state(NAN);
      return;
    }
    target->publish_state(static_cast<float>(tsdbmath::decode_aggregate(result, column.scale, column.offset)));
  };

  for (size_t index = 0; index < this->columns_.size(); index++) {
    const TsdbColumn &column = this->columns_[index];
    const auto param = static_cast<uint8_t>(index);
    if (column.average_sensor != nullptr)
      publish(column.average_sensor, param, TSDB_AGG_AVG, column);
    if (column.min_sensor != nullptr)
      publish(column.min_sensor, param, TSDB_AGG_MIN, column);
    if (column.max_sensor != nullptr)
      publish(column.max_sensor, param, TSDB_AGG_MAX, column);
  }
}

// Wall-clock seconds, or 0 when no clock is synced (record ages are then unknown).
uint32_t TsdbComponent::wall_clock_() const {
  if (this->time_ != nullptr) {
    const ESPTime now = this->time_->now();
    if (now.is_valid())
      return static_cast<uint32_t>(now.timestamp);
  }
  const uint32_t fallback = static_cast<uint32_t>(::time(nullptr));
  return tsdbmath::is_valid_timestamp(fallback) ? fallback : 0;
}

void TsdbComponent::publish_stats_() {
  auto *db = static_cast<tsdb_t *>(this->db_);
  tsdb_stats_t stats = {};
  if (tsdb_get_stats_h(db, &stats) != ESP_OK) {
    ESP_LOGW(TAG, "%s: reading the database statistics failed", this->file_.c_str());
    return;
  }
  this->records_ = stats.total_records;
  if (this->records_sensor_ != nullptr)
    this->records_sensor_->publish_state(stats.total_records);
  if (this->evictions_sensor_ != nullptr)
    this->evictions_sensor_->publish_state(stats.total_evictions);

  // Ages rather than timestamps: a uint32 epoch in a float state loses about
  // two minutes, which is more than a retention window's usable resolution.
  const uint32_t now = this->wall_clock_();
  const bool ages_known = now != 0 && stats.total_records > 0 && tsdbmath::is_valid_timestamp(stats.oldest_timestamp);
  const uint32_t oldest_age = ages_known && now > stats.oldest_timestamp ? now - stats.oldest_timestamp : 0;
  const uint32_t newest_age = ages_known && now > stats.newest_timestamp ? now - stats.newest_timestamp : 0;
  if (this->oldest_age_sensor_ != nullptr)
    this->oldest_age_sensor_->publish_state(ages_known ? static_cast<float>(oldest_age) : NAN);
  if (this->newest_age_sensor_ != nullptr)
    this->newest_age_sensor_->publish_state(ages_known ? static_cast<float>(newest_age) : NAN);
  this->publish_errors_();

  // esp_littlefs_info() walks the filesystem, so it only runs when a sensor needs it.
  size_t total = 0;
  size_t used = 0;
  if ((this->used_sensor_ != nullptr || this->free_sensor_ != nullptr) &&
      esp_littlefs_info(this->partition_label_.c_str(), &total, &used) == ESP_OK) {
    if (this->used_sensor_ != nullptr)
      this->used_sensor_->publish_state(static_cast<float>(used));
    if (this->free_sensor_ != nullptr)
      this->free_sensor_->publish_state(static_cast<float>(total - used));
  }
  ESP_LOGD(
      TAG, "%s: %" PRIu32 " records, %" PRIu32 " evicted, %" PRIu32 " writes, %" PRIu32 " dropped, %" PRIu32 " errors",
      this->file_.c_str(), this->records_, stats.total_evictions, this->writes_, this->dropped_, this->write_errors_);
}

void TsdbComponent::dump_csv_(uint32_t rows) {
  auto *db = static_cast<tsdb_t *>(this->db_);
  tsdb_stats_t stats = {};
  if (tsdb_get_stats_h(db, &stats) != ESP_OK || stats.total_records == 0) {
    ESP_LOGW(TAG, "%s: nothing to dump - the database is empty", this->file_.c_str());
    return;
  }

  const uint32_t end = stats.newest_timestamp;
  const uint32_t oldest = stats.oldest_timestamp != 0 ? stats.oldest_timestamp : 1;
  const uint32_t interval_ms = this->get_update_interval();
  const uint32_t interval_s = interval_ms >= 1000 ? interval_ms / 1000 : 1;
  const uint32_t start = tsdbmath::dump_window_start(end, oldest, rows, interval_s, DUMP_WINDOW_MARGIN);

  uint32_t window_records = 0;
  if (tsdb_query_count_h(db, start, end, &window_records) != ESP_OK) {
    ESP_LOGE(TAG, "%s: csv-dump count failed - not dumping", this->file_.c_str());
    return;
  }
  uint32_t window_start = start;
  uint32_t skip = tsdbmath::dump_skip(window_records, rows);
  if (tsdbmath::dump_needs_widening(window_records, rows, window_start, oldest)) {
    ESP_LOGW(TAG, "%s: csv-dump window holds %" PRIu32 " of %" PRIu32 " rows - widening to the whole history",
             this->file_.c_str(), window_records, rows);
    window_start = oldest;
    skip = tsdbmath::dump_skip(stats.total_records, rows);
  }

  // Column names in a stack buffer: header and rows stay heap free.
  char header[CSV_LINE_SIZE];
  int header_length = snprintf(header, sizeof(header), "timestamp");
  for (size_t index = 0;
       index < this->columns_.size() && header_length > 0 && header_length < static_cast<int>(sizeof(header));
       index++) {
    header_length += snprintf(header + header_length, sizeof(header) - static_cast<size_t>(header_length), ",%s",
                              this->columns_[index].name.c_str());
  }
  ESP_LOGI(TAG, "%s: csv-dump begin (newest %" PRIu32 " records): %s", this->file_.c_str(), rows, header);

  static tsdb_query_t query = {};
  if (tsdb_query_init_h(db, &query, window_start, end, nullptr, 0) != ESP_OK) {
    ESP_LOGE(TAG, "%s: csv-dump query failed", this->file_.c_str());
    return;
  }

  uint32_t timestamp = 0;
  // Reuse the write buffer so a dump does not allocate.
  this->values_.resize(this->columns_.size());
  uint32_t seen = 0;
  uint32_t count = 0;
  bool warned_truncation = false;
  while (count < rows && tsdb_query_next(&query, &timestamp, this->values_.data()) == ESP_OK) {
    if (seen++ < skip)
      continue;
    char line[CSV_LINE_SIZE];
    int length = snprintf(line, sizeof(line), "%" PRIu32, timestamp);
    for (size_t index = 0; index < this->values_.size() && length > 0 && length < static_cast<int>(sizeof(line));
         index++) {
      const double value =
          tsdbmath::decode(this->values_[index], this->columns_[index].scale, this->columns_[index].offset);
      length += snprintf(line + length, sizeof(line) - static_cast<size_t>(length), ",%.4f", value);
    }
    // snprintf() reports the length it would have written, so an overflowing
    // line is detectable and must not pass as a complete row.
    if (length >= static_cast<int>(sizeof(line)) && !warned_truncation) {
      warned_truncation = true;
      ESP_LOGW(TAG,
               "%s: csv-dump line longer than %" PRIu32 " bytes - truncated, lower the column count or the name length",
               this->file_.c_str(), static_cast<uint32_t>(sizeof(line)));
    }
    if (length > 0) {
      ESP_LOGI(TAG, "%s: %s", this->file_.c_str(), line);
    }
    count++;
  }
  tsdb_query_close(&query);
  ESP_LOGI(TAG, "%s: csv-dump end (%" PRIu32 " rows)", this->file_.c_str(), count);
}

void TsdbComponent::publish_log_(const char *message) {
  if (this->log_sensor_ == nullptr)
    return;
  this->log_sensor_->publish_state(message);
}

}  // namespace esphome::tsdb
