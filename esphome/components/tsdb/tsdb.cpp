#include "tsdb.h"

#include <cinttypes>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <ctime>

#include "esphome/core/helpers.h"

#include "esp_err.h"
#include "esp_littlefs.h"
#include "esp_partition.h"
#include "esp_tsdb.h"

namespace esphome::tsdb {

static const char *const TAG = "tsdb";

/// ESP_PARTITION_SUBTYPE_DATA_LITTLEFS (0x83), spelled out because the enum
/// member is not present in every ESP-IDF 5.x header.
static constexpr uint8_t LITTLEFS_SUBTYPE = 0x83;
/// Diagnostics are refreshed at most this often (milliseconds).
static constexpr uint32_t STATS_INTERVAL_MS = 60000;
/// Every Nth dropped record is logged at WARN, the rest at DEBUG.
static constexpr uint32_t DROP_LOG_EVERY = 10;
/// One line of the CSV dump (timestamp + all decoded columns).
static constexpr size_t CSV_LINE_SIZE = 256;
/// The CSV dump window covers `dump_rows * update_interval * this` seconds: the
/// extra factor absorbs timestamp jitter and a single gap in the history without
/// falling back to a scan of the whole database (see `tsdbmath::dump_window_start`).
/// A window that is short anyway is widened once to the whole history
/// (`tsdbmath::dump_needs_widening`), so the dump keeps its documented promise -
/// the newest `dump_rows` rows that are *stored*.
static constexpr uint32_t DUMP_WINDOW_MARGIN = 2;

/// The esp_tsdb free-space callback takes no context argument, so the label of
/// the mounted partition is kept here. Two tsdb instances on different
/// partitions would share it - the guard is advisory (it only caps
/// `max_records` early), so that is acceptable.
static const char *g_free_space_label = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// The host test asserts the same numbers; these tie the component to the real
// engine, so an esp_tsdb release that changes the file format breaks the build
// instead of silently invalidating docs/data_logging.md. The width used here is
// the one packages/tsdb.yaml ships (4 columns); both macros are width-agnostic.
static_assert(tsdbmath::BLOCK_SIZE == TSDB_BLOCK_SIZE, "esp_tsdb changed TSDB_BLOCK_SIZE");
static_assert(tsdbmath::BLOCK_HEADER_SIZE == TSDB_BLOCK_HEADER_SIZE, "esp_tsdb changed the block header");
static_assert(tsdbmath::records_per_block(4) == (TSDB_BLOCK_SIZE - TSDB_BLOCK_HEADER_SIZE) / (4 + 4 * 2),
              "esp_tsdb changed the record geometry");
static_assert(tsdbmath::max_records_for_bytes(3072, 4) == TSDB_CALC_MAX_RECORDS(3072, 4),
              "esp_tsdb changed TSDB_CALC_MAX_RECORDS()");
static_assert(sizeof(tsdb_header_t) == tsdbmath::HEADER_BYTES, "esp_tsdb changed sizeof(tsdb_header_t)");
static_assert(tsdbmath::FILE_MAGIC == static_cast<uint32_t>(TSDB_MAGIC), "esp_tsdb changed TSDB_MAGIC");
static_assert(tsdbmath::MAX_COLUMNS == static_cast<uint32_t>(TSDB_MAX_PARAMS), "esp_tsdb changed TSDB_MAX_PARAMS");
static_assert(offsetof(tsdb_header_t, magic) == tsdbmath::HEADER_MAGIC_OFFSET,
              "esp_tsdb moved tsdb_header_t::magic - the stored schema probe no longer reads the magic");
static_assert(offsetof(tsdb_header_t, num_params) == tsdbmath::HEADER_COLUMNS_OFFSET,
              "esp_tsdb moved tsdb_header_t::num_params - the stored schema probe no longer reads the column count");
static_assert(tsdbmath::capacity_for_bytes(384 * 1024, 4) <= TSDB_CALC_MAX_RECORDS(384 * 1024, 4),
              "the exact capacity solver may never exceed the engine's own formula");
static_assert(static_cast<int>(tsdbmath::MEMORY_INTERNAL) == static_cast<int>(TSDB_ALLOC_INTERNAL_RAM),
              "esp_tsdb changed tsdb_alloc_strategy_t");
static_assert(static_cast<int>(tsdbmath::MEMORY_PSRAM) == static_cast<int>(TSDB_ALLOC_PSRAM),
              "esp_tsdb changed tsdb_alloc_strategy_t");
static_assert(static_cast<int>(tsdbmath::MEMORY_AUTO) == static_cast<int>(TSDB_ALLOC_AUTO),
              "esp_tsdb changed tsdb_alloc_strategy_t");

/// True when the partition has never been formatted (every byte is 0xFF).
///
/// The **whole** partition is scanned, not just its first block: a partition that
/// holds data whose head happens to be erased (an interrupted write or format)
/// must not look blank, because `format_on_first_boot` would then re-format it
/// and throw a readable history away. The scan runs only when the mount failed
/// and stops at the first byte that is not 0xFF, so a partition that is in use
/// costs a single 256-byte read.
static bool partition_is_blank(const esp_partition_t *partition) {
  static constexpr size_t CHUNK = 256;
  uint8_t buffer[CHUNK];
  for (uint32_t offset = 0; offset < partition->size; offset += CHUNK) {
    const size_t remaining = static_cast<size_t>(partition->size) - offset;
    const size_t length = remaining < CHUNK ? remaining : CHUNK;
    if (esp_partition_read(partition, offset, buffer, length) != ESP_OK)
      return false;  // unreadable is not blank - and never a reason to format
    for (size_t index = 0; index < length; index++) {
      if (buffer[index] != 0xFF)
        return false;
    }
  }
  return true;
}

// ---------------------------------------------------------------------------- config
void TsdbComponent::add_column(const std::string &name, double scale, double offset) {
  TsdbColumn column;
  column.name = name;
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

// ---------------------------------------------------------------------------- lifecycle
void TsdbComponent::setup() {
  if (this->columns_.empty()) {
    ESP_LOGE(TAG, "tsdb: no columns configured");
    this->mark_failed();
    return;
  }
  if (!this->mount_filesystem_()) {
    this->mark_failed();
    return;
  }
  if (!this->open_database_()) {
    this->mark_failed();
    return;
  }
  this->publish_log_("history opened");
}

void TsdbComponent::on_shutdown() {
  if (this->db_ == nullptr)
    return;
  this->flush_(true);
  ESP_LOGI(TAG, "%s: closing on shutdown", this->file_.c_str());
  this->close_database_();
}

// ---------------------------------------------------------------------------- filesystem
bool TsdbComponent::mount_filesystem_() {
  const esp_partition_t *partition = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, static_cast<esp_partition_subtype_t>(LITTLEFS_SUBTYPE), this->partition_label_.c_str());
  if (partition == nullptr) {
    ESP_LOGE(TAG, "%s: no data/littlefs partition labelled '%s' - check the partition table (partition_size:)",
             this->partition_label_.c_str(), this->partition_label_.c_str());
    return false;
  }

  esp_vfs_littlefs_conf_t conf = {};
  conf.base_path = this->mount_point_.c_str();
  conf.partition_label = this->partition_label_.c_str();
  conf.partition = partition;
  conf.format_if_mount_failed = false;  // never reformat on a mount failure: that would discard history
  conf.dont_mount = false;
  conf.grow_on_mount = false;

  esp_err_t err = esp_vfs_littlefs_register(&conf);
  if (err != ESP_OK && this->format_on_first_boot_ && partition_is_blank(partition)) {
    ESP_LOGW(TAG, "%s: partition '%s' is unformatted - formatting it (first boot)", this->file_.c_str(),
             this->partition_label_.c_str());
    err = esp_littlefs_format(this->partition_label_.c_str());
    if (err == ESP_OK)
      err = esp_vfs_littlefs_register(&conf);
  }
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

// ---------------------------------------------------------------------------- database
/// `true` when the database file exists. A failed open of a *missing* file is not
/// a schema problem (it is the normal first boot), so the recovery below must
/// never delete anything then.
static bool database_file_exists(const std::string &path) {
  FILE *file = fopen(path.c_str(), "rb");
  if (file == nullptr)
    return false;
  fclose(file);
  return true;
}

/// Delete the database file and its two header sidecars. The engine keeps its
/// header - and with it the column set of the stored rows - in `.h0`/`.h1`, so a
/// file whose schema no longer matches the configuration is useless without them.
static void remove_database_files(const std::string &path) {
  static const char *const SUFFIXES[] = {"", ".h0", ".h1"};
  for (const char *suffix : SUFFIXES) {
    const std::string file = path + suffix;
    if (remove(file.c_str()) != 0) {
      ESP_LOGW(TAG, "%s: could not be deleted (not a file?)", file.c_str());
    }
  }
}

/// The column count the database file at `path` was written with, or 0 when it is
/// absent, unreadable or does not carry a header this build understands.
///
/// This is the *only* thing that may authorise deleting history, and only when it
/// differs from the configured count: an open failure can also come from a heap,
/// filesystem or I/O problem (esp_tsdb allocates its buffer pool before it even
/// touches the file), and those leave a healthy database that the next boot has to
/// be able to open. Missing-here (0) or matches-the-config means "keep it".
/// The whole 584-byte header is read because it is one small buffered read; the
/// probe itself only needs the first nine bytes.
static uint32_t read_stored_columns(const std::string &path) {
  FILE *file = fopen(path.c_str(), "rb");
  if (file == nullptr)
    return 0;
  uint8_t header[tsdbmath::HEADER_BYTES] = {};
  const size_t read = fread(header, 1, sizeof(header), file);
  fclose(file);
  return tsdbmath::stored_columns(header, read);
}

bool TsdbComponent::open_database_() {
  std::vector<const char *> names;
  names.reserve(this->columns_.size());
  for (const TsdbColumn &column : this->columns_)
    names.push_back(column.name.c_str());

  // The engine's own TSDB_CALC_MAX_RECORDS() ignores that a block only holds
  // (1024 - 8) / record_size records, so the file it sizes overflows the budget
  // by ~1.6 %; capacity_for_bytes() shrinks it until the file really fits.
  const uint32_t max_records = tsdbmath::capacity_for_bytes(this->max_file_size_, names.size(), this->index_stride_);
  if (max_records == 0) {
    ESP_LOGE(TAG, "%s: max_file_size (%" PRIu32 " bytes) is too small for %" PRIu32 " columns", this->file_.c_str(),
             this->max_file_size_, static_cast<uint32_t>(names.size()));
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
  config.alloc_strategy = static_cast<tsdb_alloc_strategy_t>(this->memory_mode_);
  config.use_paged_allocation = this->paged_allocation_;
  config.page_size = this->page_size_;
  config.min_free_bytes = this->min_free_bytes_;
  config.free_space_cb = this->min_free_bytes_ > 0 ? free_space_probe : nullptr;

  this->db_ = tsdb_open(&config);
  if (this->db_ == nullptr && this->recreate_on_schema_change_) {
    // A stored row carries its values, but not their names: the file's header
    // does. When it says the file was written with another column count, esp_tsdb
    // refuses to open it (it would mislabel every value) and returns NULL. Such a
    // column change is a schema change that no config value can absorb: either the
    // old rows go or the component stays failed forever (which would also take the
    // diagnostics and the buttons down with it). With `recreate_on_schema_change`
    // the file is therefore deleted and created anew with the schema below - once,
    // because the new file then matches.
    //
    // The deletion is gated on the file's *own* header saying so: an open failure
    // with an unreadable or matching schema is not a schema change, and wiping the
    // history for it (a transient out-of-memory at boot, a filesystem or flash
    // problem) would be worse than a boot without a database.
    const uint32_t stored = read_stored_columns(path);
    const uint32_t configured = static_cast<uint32_t>(names.size());
    if (stored != 0 && stored != configured) {
      ESP_LOGW(TAG,
               "%s: opening as %" PRIu32 " columns failed and the file stores %" PRIu32
               " - deleting it and starting a new database (the stored history is lost)",
               this->file_.c_str(), configured, stored);
      remove_database_files(path);
      this->db_ = tsdb_open(&config);
      if (this->db_ != nullptr)
        this->publish_log_("history recreated (the column set changed)");
    } else if (database_file_exists(path)) {
      ESP_LOGW(TAG,
               "%s: opening failed but the stored schema is %s - keeping the file (only a different column set "
               "is a reason to delete history)",
               this->file_.c_str(),
               stored == 0 ? LOG_STR_LITERAL("unreadable") : LOG_STR_LITERAL("the configured one"));
    }
  }
  if (this->db_ == nullptr) {
    ESP_LOGE(TAG, "%s: esp_tsdb could not open %s (buffer %" PRIu32 " bytes, memory mode %u)", this->file_.c_str(),
             path.c_str(), this->buffer_pool_size_, static_cast<unsigned>(this->memory_mode_));
    return false;
  }

  const uint64_t bytes = tsdbmath::file_bytes_for(max_records, names.size(), this->index_stride_);
  ESP_LOGI(TAG,
           "%s: %" PRIu32 " columns, %" PRIu32 " records (%" PRIu32 " bytes of data, up to %" PRIu32
           " KB of file) at %s",
           this->file_.c_str(), static_cast<uint32_t>(names.size()), max_records, static_cast<uint32_t>(bytes),
           this->max_file_size_ / 1024, path.c_str());
  if (this->min_free_bytes_ > 0) {
    ESP_LOGI(TAG, "%s: capacity is capped early when %s drops below %" PRIu32 " bytes", this->file_.c_str(),
             this->partition_label_.c_str(), this->min_free_bytes_);
  }
  return true;
}

void TsdbComponent::close_database_() {
  if (this->db_ == nullptr)
    return;
  tsdb_close_h(static_cast<tsdb_t *>(this->db_));
  this->db_ = nullptr;  // a closed handle must not be reused
}

/// Free bytes of the mounted partition, probed by the engine before it grows the
/// file. UINT64_MAX means "unknown", which never triggers a spurious cap.
uint64_t TsdbComponent::free_space_probe() {
  if (g_free_space_label == nullptr)
    return UINT64_MAX;
  size_t total = 0;
  size_t used = 0;
  if (esp_littlefs_info(g_free_space_label, &total, &used) != ESP_OK)
    return UINT64_MAX;
  return static_cast<uint64_t>(total) - static_cast<uint64_t>(used);
}

// ---------------------------------------------------------------------------- runtime
void TsdbComponent::update() {
  if (this->db_ == nullptr)
    return;

  this->handle_requests_();
  // flush_() keeps its own `sync_interval` deadline, so it is called after every
  // attempt - including an attempt that dropped the row. Returning early on a
  // dropped row (the old `if (write_record_())`) is what let the last written
  // record sit unsynced for longer than `sync_interval` while the history had a
  // gap.
  this->write_record_();
  this->flush_(false);

  const uint32_t now = millis();
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
      ESP_LOGE(TAG, "%s: clearing the history failed: %s", this->file_.c_str(), esp_err_to_name(err));
    } else {
      this->records_ = 0;
      this->writes_ = 0;
      // tsdb_clear_h() flushes and syncs the header itself, so nothing stored is
      // left uncommitted: an extra commit at the next deadline would publish an
      // unchanged file.
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
  uint32_t timestamp = 0;
  if (this->time_ != nullptr) {
    const ESPTime now = this->time_->now();
    if (now.is_valid())
      timestamp = static_cast<uint32_t>(now.timestamp);
  } else {
    timestamp = static_cast<uint32_t>(::time(nullptr));
  }

  if (this->require_time_ && !tsdbmath::is_valid_timestamp(timestamp)) {
    if (!this->warned_time_) {
      this->warned_time_ = true;
      ESP_LOGW(TAG, "%s: no valid time yet - no record is written until the clock is set", this->file_.c_str());
      this->publish_log_("waiting for a valid time");
    }
    return false;
  }
  this->warned_time_ = false;

  std::vector<int16_t> values;
  values.reserve(this->columns_.size());
  for (TsdbColumn &column : this->columns_) {
    const bool has_value =
        column.source != nullptr && column.source->has_state() && !tsdbmath::is_missing(column.source->state);
    if (has_value) {
      column.held = tsdbmath::encode(column.source->state, column.scale, column.offset);
      values.push_back(column.held);
      continue;
    }
    if (this->missing_policy_ == tsdbmath::MISSING_HOLD && column.held != tsdbmath::RAW_UNKNOWN) {
      values.push_back(column.held);
      continue;
    }
    if (this->missing_policy_ == tsdbmath::MISSING_SENTINEL) {
      values.push_back(tsdbmath::RAW_UNKNOWN);
      continue;
    }
    // MISSING_SKIP (default): a partial row is worse than a gap, so the whole
    // record is dropped - and so is a row that would look like clean air.
    this->dropped_++;
    if (this->dropped_ == 1 || this->dropped_ % DROP_LOG_EVERY == 0) {
      ESP_LOGW(TAG, "%s: %s has no value - record dropped (%" PRIu32 " dropped so far)", this->file_.c_str(),
               column.name.c_str(), this->dropped_);
      this->publish_log_("record dropped (a column had no value)");
    } else {
      ESP_LOGD(TAG, "%s: %s has no value - record dropped", this->file_.c_str(), column.name.c_str());
    }
    return false;
  }

  const esp_err_t err = tsdb_write_h(static_cast<tsdb_t *>(this->db_), timestamp, values.data());
  if (err != ESP_OK) {
    this->write_errors_++;
    ESP_LOGE(TAG, "%s: writing a record failed: %s", this->file_.c_str(), esp_err_to_name(err));
    if (this->errors_sensor_ != nullptr)
      this->errors_sensor_->publish_state(this->write_errors_);
    return false;
  }
  this->writes_++;
  this->dirty_ = true;  // flush_() may not have committed this record yet
  return true;
}

void TsdbComponent::flush_(bool force) {
  if (this->db_ == nullptr)
    return;
  const uint32_t now = millis();
  // Nothing was written since the last commit: syncing anyway would spend a flash
  // metadata commit to publish an unchanged file (an idle update interval, or a
  // run of dropped rows).
  if (!force && !this->dirty_)
    return;
  // The first write always syncs: that is the commit which gives the file its
  // directory entry, i.e. what makes the database survive a reboot at all.
  if (!force && this->last_sync_ != 0 && this->sync_interval_ > 0 && now - this->last_sync_ < this->sync_interval_)
    return;
  this->last_sync_ = now == 0 ? 1 : now;

  const esp_err_t err = tsdb_sync_h(static_cast<tsdb_t *>(this->db_));
  if (err != ESP_OK) {
    this->write_errors_++;
    ESP_LOGE(TAG, "%s: sync failed: %s", this->file_.c_str(), esp_err_to_name(err));
    if (this->errors_sensor_ != nullptr)
      this->errors_sensor_->publish_state(this->write_errors_);
    return;  // dirty_ stays set: the next deadline retries the commit
  }
  this->dirty_ = false;
  ESP_LOGD(TAG, "%s: synced after %" PRIu32 " writes", this->file_.c_str(), this->writes_);
}

// ---------------------------------------------------------------------------- aggregates
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
  // Without a record in the window there is nothing to aggregate - the sensors
  // go to `unknown` (NaN) instead of a misleading 0.
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

// ---------------------------------------------------------------------------- diagnostics
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
  if (this->oldest_sensor_ != nullptr)
    this->oldest_sensor_->publish_state(stats.total_records > 0 ? static_cast<float>(stats.oldest_timestamp) : NAN);
  if (this->newest_sensor_ != nullptr)
    this->newest_sensor_->publish_state(stats.total_records > 0 ? static_cast<float>(stats.newest_timestamp) : NAN);
  if (this->errors_sensor_ != nullptr)
    this->errors_sensor_->publish_state(this->write_errors_);

  size_t total = 0;
  size_t used = 0;
  if (esp_littlefs_info(this->partition_label_.c_str(), &total, &used) == ESP_OK) {
    if (this->used_sensor_ != nullptr)
      this->used_sensor_->publish_state(static_cast<float>(used));
    if (this->free_sensor_ != nullptr)
      this->free_sensor_->publish_state(static_cast<float>(total - used));
  }
  ESP_LOGD(TAG, "%s: %" PRIu32 " records of %" PRIu32 ", %" PRIu32 " writes, %" PRIu32 " dropped, %" PRIu32 " errors",
           this->file_.c_str(), this->records_, stats.max_records, this->writes_, this->dropped_, this->write_errors_);
}

// ---------------------------------------------------------------------------- export
void TsdbComponent::dump_csv_(uint32_t rows) {
  auto *db = static_cast<tsdb_t *>(this->db_);
  tsdb_stats_t stats = {};
  if (tsdb_get_stats_h(db, &stats) != ESP_OK || stats.total_records == 0) {
    ESP_LOGW(TAG, "%s: nothing to dump - the database is empty", this->file_.c_str());
    return;
  }

  // The window is selected by *time* so the engine can seek to it in O(log n)
  // block reads: the query only skips its start timestamp forward in the index
  // while that start is later than the oldest record, so asking it for the
  // newest `rows` records by starting at the oldest one made every dump walk -
  // and discard - the whole history, stalling the loop (and the 1 s readings)
  // for the duration. The window is `rows` write intervals back from the newest
  // record, times DUMP_WINDOW_MARGIN so timestamp jitter and a single gap do not
  // leave it short. A window that is short anyway is widened once to the whole
  // history below, so the dump still prints the newest `rows` *stored* rows.
  const uint32_t end = stats.newest_timestamp;
  const uint32_t oldest = stats.oldest_timestamp != 0 ? stats.oldest_timestamp : 1;
  const uint32_t interval_ms = this->get_update_interval();
  const uint32_t interval_s = interval_ms >= 1000 ? interval_ms / 1000 : 1;
  const uint32_t start = tsdbmath::dump_window_start(end, oldest, rows, interval_s, DUMP_WINDOW_MARGIN);

  // The margin over-provisions the window, so it can hold a few rows more than
  // asked for; `skip` trims those to the newest `rows`.
  uint32_t window_records = 0;
  if (tsdb_query_count_h(db, start, end, &window_records) != ESP_OK) {
    // Without the count the newest rows cannot be told from the oldest ones of
    // the over-provisioned window: dumping anyway would print old rows as the
    // newest ones, so the dump fails instead.
    ESP_LOGE(TAG, "%s: csv-dump count failed - not dumping (the window cannot be trimmed to the newest rows)",
             this->file_.c_str());
    return;
  }
  uint32_t window_start = start;
  uint32_t skip = tsdbmath::dump_skip(window_records, rows);
  if (tsdbmath::dump_needs_widening(window_records, rows, window_start, oldest)) {
    // A gap wider than the margin left the time-selected window with fewer rows
    // than asked for although older rows exist (`on_missing: skip` writes nothing
    // while a sensor is out). Widening once to the oldest record makes the dump
    // exact; the range then *is* the whole ring, so its row count is the
    // `total_records` of the statistics above - no second count pass is needed.
    ESP_LOGW(TAG,
             "%s: csv-dump window holds %" PRIu32 " of %" PRIu32 " rows (a gap) - widening it to the whole history",
             this->file_.c_str(), window_records, rows);
    window_start = oldest;
    skip = tsdbmath::dump_skip(stats.total_records, rows);
  }

  std::string header = "timestamp";
  for (const TsdbColumn &column : this->columns_) {
    header += ',';
    header += column.name;
  }
  ESP_LOGI(TAG, "%s: csv-dump begin (newest %" PRIu32 " records, window [%" PRIu32 ", %" PRIu32 "], %s ...)",
           this->file_.c_str(), rows, window_start, end, header.c_str());

  // The query state is ~700 bytes; a function local static keeps it out of the
  // (small) loop task stack - the dump runs there, on request only.
  static tsdb_query_t query = {};
  if (tsdb_query_init_h(db, &query, window_start, end, nullptr, 0) != ESP_OK) {
    ESP_LOGE(TAG, "%s: csv-dump query failed", this->file_.c_str());
    return;
  }

  uint32_t timestamp = 0;
  std::vector<int16_t> values(this->columns_.size(), 0);
  uint32_t seen = 0;
  uint32_t count = 0;
  while (count < rows && tsdb_query_next(&query, &timestamp, values.data()) == ESP_OK) {
    if (seen++ < skip)
      continue;  // an older record of the over-provisioned window
    char line[CSV_LINE_SIZE];
    int length = snprintf(line, sizeof(line), "%" PRIu32, timestamp);
    for (size_t index = 0; index < values.size() && length > 0 && length < static_cast<int>(sizeof(line)); index++) {
      const double value = tsdbmath::decode(values[index], this->columns_[index].scale, this->columns_[index].offset);
      length += snprintf(line + length, sizeof(line) - static_cast<size_t>(length), ",%.4f", value);
    }
    if (length > 0) {
      ESP_LOGI(TAG, "%s: %s", this->file_.c_str(), line);
    }
    count++;
  }
  tsdb_query_close(&query);
  if (count < rows) {
    // The window can only be short now when the database itself holds fewer rows
    // than asked for (a young database): every gap that older rows could cover
    // was widened above.
    ESP_LOGW(TAG, "%s: csv-dump got %" PRIu32 " of %" PRIu32 " rows (the database holds %" PRIu32 ")",
             this->file_.c_str(), count, rows, stats.total_records);
  }
  ESP_LOGI(TAG, "%s: csv-dump end (%" PRIu32 " rows, %" PRIu32 " older skipped)", this->file_.c_str(), count, skip);
}

void TsdbComponent::publish_log_(const char *message) {
  if (this->log_sensor_ == nullptr)
    return;
  this->log_sensor_->publish_state(message);
}

// ---------------------------------------------------------------------------- config dump
void TsdbComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "tsdb:");
  ESP_LOGCONFIG(TAG, "  file: %s/%s", this->mount_point_.c_str(), this->file_.c_str());
  ESP_LOGCONFIG(TAG, "  partition: %s, %" PRIu32 " KB requested (the two app slots share the rest)",
                this->partition_label_.c_str(), this->partition_size_ / 1024);
  ESP_LOGCONFIG(TAG, "  columns: %" PRIu32, static_cast<uint32_t>(this->columns_.size()));
  for (const TsdbColumn &column : this->columns_) {
    ESP_LOGCONFIG(TAG, "    %s: scale %.6g, offset %.6g", column.name.c_str(), column.scale, column.offset);
  }
  ESP_LOGCONFIG(TAG, "  write interval: %" PRIu32 " ms, sync interval: %" PRIu32 " ms, data budget: %" PRIu32 " KB",
                this->get_update_interval(), this->sync_interval_, this->max_file_size_ / 1024);
  // Deliberately numeric: a `?:` of two string literals inside a log call is what
  // the ESPHome linter flags (bare literals stay in RAM on ESP8266).
  ESP_LOGCONFIG(TAG, "  buffer pool: %" PRIu32 " bytes (paged: %u), memory mode: %u, min free: %" PRIu32 " bytes",
                this->buffer_pool_size_, static_cast<unsigned>(this->paged_allocation_),
                static_cast<unsigned>(this->memory_mode_), this->min_free_bytes_);
  ESP_LOGCONFIG(TAG, "  missing columns: %u, require time: %u", static_cast<unsigned>(this->missing_policy_),
                static_cast<unsigned>(this->require_time_));
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  the database is NOT available (see the errors above)");
    return;
  }
  ESP_LOGCONFIG(TAG, "  records: %" PRIu32 ", writes: %" PRIu32 ", dropped: %" PRIu32 ", errors: %" PRIu32,
                this->records_, this->writes_, this->dropped_, this->write_errors_);
}

}  // namespace esphome::tsdb
