#include "tsdb.h"

#ifdef USE_ESP_IDF

#include <algorithm>
#include <array>
#include <cerrno>
#include <cinttypes>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <ctime>
#include <sys/stat.h>

#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
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
static constexpr uint32_t DROP_LOG_EVERY = 10;
// Rate limit for the free space probe failures.
static constexpr uint32_t PROBE_LOG_EVERY = 100;
// The engine keeps its durable header slots beside the database, in two layouts.
static constexpr const char *const SIDECAR_SUFFIXES[] = {".h0", ".h1", ".d/h0", ".d/h1"};
// A shutdown gives up on a sync task that does not finish in this time.
static constexpr uint32_t SHUTDOWN_SYNC_TIMEOUT_MS = 30000;
#ifdef USE_TSDB_DUMP
// One CSV dump line.
static constexpr size_t CSV_LINE_SIZE = 256;
// "timestamp" plus one maximum-length name per column with its comma and terminator.
static constexpr size_t CSV_HEADER_SIZE =
    (sizeof("timestamp") - 1) + tsdbmath::MAX_BASE_PARAMS * (1 + tsdbmath::MAX_NAME_LENGTH) + 1;
// The logger drops what does not fit its line buffer and writes the level and tag in
// front of the line, so a header can only be dumped when it fits what remains.
#ifdef ESPHOME_LOGGER_TX_BUFFER_SIZE
static constexpr size_t LOG_LINE_SIZE = ESPHOME_LOGGER_TX_BUFFER_SIZE;
#else
static constexpr size_t LOG_LINE_SIZE = 512;
#endif
static constexpr size_t LOG_LINE_OVERHEAD = 64;
static constexpr size_t CSV_HEADER_LIMIT =
    LOG_LINE_SIZE > LOG_LINE_OVERHEAD ? std::min(CSV_HEADER_SIZE, LOG_LINE_SIZE - LOG_LINE_OVERHEAD) : 0;
// Dump window margin for jitter and single gaps.
static constexpr uint32_t DUMP_WINDOW_MARGIN = 2;
#endif
static constexpr uint32_t REOPEN_RETRY_MS = 300000;
// The sync task only folds the header into the file: 4 KB of stack is enough for the
// stdio and littlefs calls, and it runs below the main loop.
static constexpr uint32_t SYNC_TASK_STACK_SIZE = 4096;
static constexpr UBaseType_t SYNC_TASK_PRIORITY = 1;

// dump_config() names, indexed by the tsdbmath enums (last entry is the fallback).
PROGMEM_STRING_TABLE(MissingPolicyStrings, "skip", "hold", "sentinel", "unknown");
PROGMEM_STRING_TABLE(MemoryModeStrings, "internal", "psram", "auto", "unknown");

static const LogString *missing_policy_to_string(tsdbmath::MissingPolicy policy) {
  return MissingPolicyStrings::get_log_str(static_cast<uint8_t>(policy), MissingPolicyStrings::LAST_INDEX);
}

static const LogString *memory_mode_to_string(tsdbmath::MemoryMode mode) {
  return MemoryModeStrings::get_log_str(static_cast<uint8_t>(mode), MemoryModeStrings::LAST_INDEX);
}

// The esp_tsdb free-space callback takes no context: the mounted label and the count of
// failed probes (to rate limit the warning) live in one function-local instance.
struct FreeSpaceProbe {
  const char *label{nullptr};
  uint32_t failures{0};
};

static FreeSpaceProbe &free_space_state() {
  static FreeSpaceProbe state;
  return state;
}

static_assert(tsdbmath::BLOCK_SIZE == TSDB_BLOCK_SIZE, "esp_tsdb changed TSDB_BLOCK_SIZE");
static_assert(tsdbmath::BLOCK_HEADER_SIZE == TSDB_BLOCK_HEADER_SIZE, "esp_tsdb changed the block header");
static_assert(tsdbmath::records_per_block(4) == (TSDB_BLOCK_SIZE - TSDB_BLOCK_HEADER_SIZE) / (4 + 4 * 2),
              "esp_tsdb changed the record geometry");
static_assert(tsdbmath::max_records_for_bytes(3072, 4) == TSDB_CALC_MAX_RECORDS(3072, 4),
              "esp_tsdb changed TSDB_CALC_MAX_RECORDS()");
static_assert(sizeof(tsdb_header_t) == tsdbmath::HEADER_BYTES, "esp_tsdb changed sizeof(tsdb_header_t)");
static_assert(tsdbmath::HEADER_BYTES == TSDB_V4_EXT_NAMES_OFFSET, "esp_tsdb changed the header size");
static_assert(tsdbmath::FILE_MAGIC == static_cast<uint32_t>(TSDB_MAGIC), "esp_tsdb changed TSDB_MAGIC");
static_assert(tsdbmath::MAX_COLUMNS == static_cast<uint32_t>(TSDB_MAX_PARAMS), "esp_tsdb changed TSDB_MAX_PARAMS");
static_assert(offsetof(tsdb_header_t, magic) == tsdbmath::HEADER_MAGIC_OFFSET, "esp_tsdb moved magic");
static_assert(offsetof(tsdb_header_t, num_params) == tsdbmath::HEADER_COLUMNS_OFFSET, "esp_tsdb moved num_params");
static_assert(offsetof(tsdb_header_t, index_stride) + sizeof(uint32_t) <= tsdbmath::HEADER_BYTES,
              "esp_tsdb moved index_stride out of the header");
static_assert(tsdbmath::capacity_for_bytes(384 * 1024, 4) <= TSDB_CALC_MAX_RECORDS(384 * 1024, 4),
              "capacity solver may never exceed the engine formula");
static_assert(static_cast<int>(tsdbmath::MEMORY_INTERNAL) == static_cast<int>(TSDB_ALLOC_INTERNAL_RAM),
              "esp_tsdb changed tsdb_alloc_strategy_t");
static_assert(static_cast<int>(tsdbmath::MEMORY_PSRAM) == static_cast<int>(TSDB_ALLOC_PSRAM),
              "esp_tsdb changed tsdb_alloc_strategy_t");
static_assert(static_cast<int>(tsdbmath::MEMORY_AUTO) == static_cast<int>(TSDB_ALLOC_AUTO),
              "esp_tsdb changed tsdb_alloc_strategy_t");
static_assert(sizeof(tsdb_block_t) == tsdbmath::BLOCK_IN_MEMORY_BYTES, "esp_tsdb changed sizeof(tsdb_block_t)");
static_assert(tsdbmath::SIDECAR1_BYTES == 12 + sizeof(tsdb_header_t), "esp_tsdb changed the TSH1 slot");
static_assert(tsdbmath::SIDECAR2_BYTES == 16 + offsetof(tsdb_header_t, param_names) +
                                              (sizeof(tsdb_header_t) - offsetof(tsdb_header_t, base_params)),
              "esp_tsdb changed the TSH2 slot");

// The engine's own header check (tsdb_header_is_sane), used to tell a readable history
// from one tsdb_open() would unlink before any schema check.
static bool header_is_sane(const tsdb_header_t &header) {
  if (header.num_params == 0 || header.num_params > TSDB_MAX_PARAMS)
    return false;
  if (header.num_params > 16 && header.version < TSDB_VERSION_WIDE)
    return false;
  if (header.base_params != 0 && header.base_params > header.num_params)
    return false;
  if (header.param_size != 0 && header.param_size != 2)
    return false;
  const uint16_t record_size = static_cast<uint16_t>(4 + header.num_params * 2);
  if (header.record_size != 0 && header.record_size != record_size)
    return false;
  const uint16_t max_records_per_block =
      static_cast<uint16_t>((tsdbmath::BLOCK_SIZE - tsdbmath::BLOCK_HEADER_SIZE) / record_size);
  if (header.records_per_block == 0 || header.records_per_block > max_records_per_block)
    return false;
  if (header.index_offset < 512 || header.index_offset > 0x10000)
    return false;
  if (header.index_entries > 0x100000)
    return false;
  if (header.index_stride == 0)
    return false;
  return header.max_records <= 100000000UL;
}

static bool partition_is_blank(const esp_partition_t *partition) {
  static constexpr size_t chunk = 256;
  uint8_t buffer[chunk];
  for (uint32_t offset = 0; offset < partition->size; offset += chunk) {
    const size_t remaining = static_cast<size_t>(partition->size) - offset;
    const size_t length = remaining < chunk ? remaining : chunk;
    const esp_err_t err = esp_partition_read(partition, offset, buffer, length);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "partition read at offset %" PRIu32 " failed: %s", offset, esp_err_to_name(err));
      return false;
    }
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

PathBuffer TsdbComponent::format_path_() const {
  PathBuffer buffer{};
  snprintf(buffer.data(), buffer.size(), "%s/%s", this->mount_point_.c_str(), this->file_.c_str());
  return buffer;
}

// The engine keeps two alternating durable header copies beside the database: the legacy
// <file>.h0/.h1 and, from 2.4.4, <file>.d/h0 and h1. It adopts the newer one on open when
// it describes at least as many records.
bool TsdbComponent::remove_sidecars_() {
  const PathBuffer path_buffer = this->format_path_();
  const char *path = path_buffer.data();
  bool removed = true;
  for (const char *suffix : SIDECAR_SUFFIXES) {
    char sidecar[PATH_BUFFER_SIZE + 8];
    snprintf(sidecar, sizeof(sidecar), "%s%s", path, suffix);
    if (remove(sidecar) != 0 && errno != ENOENT) {
      ESP_LOGE(TAG, "%s: deleting %s failed", this->file_.c_str(), sidecar);
      removed = false;
    }
  }
  return removed;
}

// Both durable header slot formats start with {uint32 magic; uint32 seq}. The engine
// ignores the result of its slot write, so a record that returns OK can still be
// missing from the durable header: require the newest slot's sequence to advance.
// Only a complete slot with a matching CRC counts, since that is all a reopen accepts.
bool TsdbComponent::sidecar_advanced_() {
  const uint32_t newest = this->newest_sidecar_seq_on_disk_();
  if (newest == 0 || newest <= this->newest_sidecar_seq_)
    return false;
  this->newest_sidecar_seq_ = newest;
  return true;
}

uint32_t TsdbComponent::newest_sidecar_seq_on_disk_() const {
  const PathBuffer path_buffer = this->format_path_();
  const char *path = path_buffer.data();
  uint32_t newest = 0;
  for (const char *suffix : SIDECAR_SUFFIXES) {
    char slot[PATH_BUFFER_SIZE + 8];
    snprintf(slot, sizeof(slot), "%s%s", path, suffix);
    FILE *file = fopen(slot, "rb");
    if (file == nullptr)
      continue;
    uint8_t buffer[tsdbmath::SIDECAR1_BYTES];  // the larger of the two slot formats
    const size_t read = fread(buffer, 1, sizeof(buffer), file);
    fclose(file);
    const uint32_t seq = tsdbmath::sidecar_slot_seq(buffer, read);
    if (seq > newest)
      newest = seq;
  }
  return newest;
}

// The header as it currently is on disk, independent of the engine's in-memory copy.
bool TsdbComponent::read_disk_header_(tsdb_header_t *header) const {
  const PathBuffer path_buffer = this->format_path_();
  const char *path = path_buffer.data();
  FILE *file = fopen(path, "rb");
  if (file == nullptr)
    return false;
  tsdb_header_t persisted = {};
  const size_t read = fread(&persisted, 1, sizeof(persisted), file);
  fclose(file);
  if (read != sizeof(persisted))
    return false;
  // Writing through the parameter keeps it a genuine output argument.
  *header = persisted;
  return true;
}

// A fold is only durable when the file's header describes what the engine holds in
// memory; tsdb_sync_h() returns OK even when its header write failed.
bool TsdbComponent::folded_header_matches_() {
  tsdb_header_t persisted = {};
  if (!this->read_disk_header_(&persisted) || persisted.magic != static_cast<uint32_t>(TSDB_MAGIC))
    return false;
  tsdb_stats_t stats = {};
  if (tsdb_get_stats_h(static_cast<tsdb_t *>(this->db_), &stats) != ESP_OK)
    return false;
  // total_records is clamped to the ring capacity in the stats, so compare the fields
  // the engine copies straight from the header.
  return persisted.total_writes == stats.total_writes && persisted.total_evictions == stats.total_evictions &&
         persisted.newest_timestamp == stats.newest_timestamp && persisted.max_records == stats.max_records;
}

void TsdbComponent::setup() {
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
  if (this->db_ != nullptr) {
    this->publish_log_("history opened");
    // Report the diagnostics now instead of leaving them unknown until the first
    // stats tick.
    this->last_stats_ = millis();
    this->publish_stats_();
  }
  // The periodic fold rewrites the whole file (seconds on a large history), so it runs
  // on its own task instead of stalling the main loop. Without it the syncs stay inline.
  if (!this->sync_task_.create(TsdbComponent::sync_task_fn, "tsdb_sync", SYNC_TASK_STACK_SIZE, this, SYNC_TASK_PRIORITY,
                               false)) {
    ESP_LOGW(TAG, "%s: no sync task - folding the history on the main loop", this->file_.c_str());
  }
}

// Count the failed initialisation so the diagnostics sensor reports it instead
// of staying "unknown" on a device whose database never opened.
void TsdbComponent::fail_setup_(const LogString *reason) {
  this->write_errors_++;
  this->publish_errors_();
  this->mark_failed(reason);
}

void TsdbComponent::on_shutdown() {
  // Wait for an in-flight sync, then fold once more inline: a shutdown may block.
  if (!this->wait_for_sync_()) {
    ESP_LOGE(TAG, "%s: the sync task did not finish - leaving the database open", this->file_.c_str());
    return;
  }
  if (this->db_ == nullptr) {
    this->unmount_filesystem_();
    return;
  }
  if (this->dirty_)
    this->sync_now_();
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
  free_space_state().label = this->partition_label_.c_str();
  size_t total = 0;
  size_t used = 0;
  if (esp_littlefs_info(this->partition_label_.c_str(), &total, &used) == ESP_OK) {
    ESP_LOGI(TAG, "%s: %s mounted on %s (%" PRIu32 " of %" PRIu32 " bytes used)", this->file_.c_str(),
             this->partition_label_.c_str(), this->mount_point_.c_str(), static_cast<uint32_t>(used),
             static_cast<uint32_t>(total));
  }
  return true;
}

static bool database_file_exists(const char *path) {
  FILE *file = fopen(path, "rb");
  if (file == nullptr)
    return false;
  fclose(file);
  return true;
}

// Stored column count, or 0 when absent/unreadable (never a delete reason).
static uint32_t read_stored_columns(const char *path) {
  FILE *file = fopen(path, "rb");
  if (file == nullptr)
    return 0;
  uint8_t header[tsdbmath::HEADER_BYTES] = {};
  const size_t read = fread(header, 1, sizeof(header), file);
  fclose(file);
  return tsdbmath::stored_columns(header, read);
}

// One stat() covers both the presence and the size of the database file.
void TsdbComponent::log_init_failure_() {
  const PathBuffer path_buffer = this->format_path_();
  const char *path = path_buffer.data();
  const esp_partition_t *partition = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, static_cast<esp_partition_subtype_t>(LITTLEFS_SUBTYPE), this->partition_label_.c_str());
  if (partition == nullptr) {
    ESP_LOGE(TAG, "  partition '%s': NOT FOUND", this->partition_label_.c_str());
  } else {
    ESP_LOGE(TAG, "  partition '%s': at 0x%08" PRIx32 ", %" PRIu32 " bytes", partition->label,
             static_cast<uint32_t>(partition->address), static_cast<uint32_t>(partition->size));
  }
  struct stat info = {};
  const bool present = ::stat(path, &info) == 0;
  ESP_LOGE(TAG, "  mount: %s, %s: %s", esp_err_to_name(this->mount_error_), path,
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

void TsdbComponent::unmount_filesystem_() {
  if (!this->mounted_)
    return;
  const esp_err_t err = esp_vfs_littlefs_unregister(this->partition_label_.c_str());
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "%s: unmounting the filesystem failed: %s", this->file_.c_str(), esp_err_to_name(err));
  }
  this->mounted_ = false;
  free_space_state().label = nullptr;
}

HistoryState TsdbComponent::inspect_history_(const char *path, const LogString **reason) const {
  FILE *file = fopen(path, "rb");
  if (file == nullptr) {
    if (errno == ENOENT)
      return HistoryState::HISTORY_STATE_MISSING;
    *reason = LOG_STR("cannot be opened");
    return HistoryState::HISTORY_STATE_UNREADABLE;
  }
  tsdb_header_t header = {};
  const size_t read = fread(&header, 1, sizeof(header), file);
  fclose(file);
  if (read == 0) {
    // An empty file holds no history; the engine recreates it.
    return HistoryState::HISTORY_STATE_MISSING;
  }
  if (read < sizeof(header)) {
    *reason = LOG_STR("shorter than its header");
    return HistoryState::HISTORY_STATE_UNREADABLE;
  }
  if (header.magic != static_cast<uint32_t>(TSDB_MAGIC)) {
    *reason = LOG_STR("header magic mismatch");
    return HistoryState::HISTORY_STATE_UNREADABLE;
  }
  if (header.version == 99) {
    *reason = LOG_STR("interrupted layout migration");
    return HistoryState::HISTORY_STATE_UNREADABLE;
  }
  if (!header_is_sane(header)) {
    *reason = LOG_STR("implausible header fields");
    return HistoryState::HISTORY_STATE_UNREADABLE;
  }
  return HistoryState::HISTORY_STATE_READABLE;
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
  const PathBuffer path_buffer = this->format_path_();
  const char *path = path_buffer.data();

  // tsdb_open() unlinks a history file it cannot read or reconstruct, before any schema
  // check. Inspect it first so only a column mismatch the config allows may delete it.
  const LogString *unreadable = nullptr;
  if (this->inspect_history_(path, &unreadable) == HistoryState::HISTORY_STATE_UNREADABLE) {
    ESP_LOGE(TAG, "%s: not opening an unreadable history (%s) - delete %s to start over", this->file_.c_str(),
             LOG_STR_ARG(unreadable), path);
    return false;
  }

  tsdb_config_t config = {};
  config.filepath = path;
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
      if (remove(path) != 0 && errno != ENOENT) {
        ESP_LOGW(TAG, "%s: could not be deleted", path);
      }
      // The durable header slots would otherwise bring the old schema's history back.
      this->remove_sidecars_();
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
             this->file_.c_str(), path, columns, max_records);
    return false;
  }

  tsdb_stats_t stats = {};
  if (tsdb_get_stats_h(static_cast<tsdb_t *>(this->db_), &stats) != ESP_OK) {
    this->invalidate_database_(LOG_STR("the stored statistics could not be read - no record can be ordered"));
    return true;
  }
  // The guard only moves forward: seed it from the newest stored record, never lower it on a reopen.
  this->last_timestamp_ = std::max(this->last_timestamp_, stats.newest_timestamp);
  this->uptime_offset_ = this->last_timestamp_ + 1 - static_cast<uint32_t>(::time(nullptr));
  this->sync_failed_ = false;
  uint32_t saved_stride = 0;
  const bool have_stride = this->read_saved_index_stride_(&saved_stride);
  const uint32_t records = stats.max_records > 0 ? stats.max_records : max_records;
  const uint32_t stride = have_stride ? saved_stride : this->index_stride_;
  const uint64_t bytes = tsdbmath::file_bytes_for(records, columns, stride);
  ESP_LOGI(TAG, "%s: %" PRIu32 " columns, %" PRIu32 " records (up to %" PRIu32 " KB) at %s", this->file_.c_str(),
           static_cast<uint32_t>(names.size()), records, static_cast<uint32_t>(bytes / 1024), path);
  if (records != max_records) {
    ESP_LOGW(TAG,
             "%s: saved capacity is %" PRIu32 " records, the configured budget asks for %" PRIu32
             " - recreate the history to change it",
             this->file_.c_str(), records, max_records);
  }
  if (have_stride && saved_stride != this->index_stride_) {
    ESP_LOGW(TAG,
             "%s: saved index stride is %" PRIu32 ", the configured value is %" PRIu32
             " - recreate the history to change it",
             this->file_.c_str(), saved_stride, this->index_stride_);
  }
  // Seed the durable-slot baseline from what is on disk now (an unclean shutdown leaves
  // slots the engine will keep using), so the next write must really advance it.
  this->newest_sidecar_seq_ = this->newest_sidecar_seq_on_disk_();
  return true;
}

bool TsdbComponent::read_saved_index_stride_(uint32_t *stride) const {
  const PathBuffer path_buffer = this->format_path_();
  const char *path = path_buffer.data();
  FILE *file = fopen(path, "rb");
  const int32_t field = static_cast<int32_t>(offsetof(tsdb_header_t, index_stride));
  uint32_t value = 0;
  const bool ok = file != nullptr && fseek(file, field, SEEK_SET) == 0 && fread(&value, sizeof(value), 1, file) == 1;
  if (file != nullptr)
    fclose(file);
  if (!ok || value == 0) {
    ESP_LOGW(TAG, "%s: saved index stride %s - using the configured %" PRIu32, this->file_.c_str(),
             ok ? LOG_STR_LITERAL("is 0") : LOG_STR_LITERAL("could not be read"), this->index_stride_);
    return false;
  }
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

// A failed sync closes the handle and every later write returns ESP_ERR_INVALID_STATE.
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
  const uint32_t now = App.get_loop_component_start_time();
  if (this->last_reopen_ != 0 && now - this->last_reopen_ < REOPEN_RETRY_MS)
    return;
  this->last_reopen_ = now == 0 ? 1 : now;
  if (!this->open_database_() || this->db_ == nullptr) {
    this->reopen_errors_++;
    return;
  }
  this->db_lost_ = false;
  ESP_LOGI(TAG, "%s: history reopened", this->file_.c_str());
  this->status_clear_warning();
}

uint64_t TsdbComponent::free_space_probe() {
  size_t total = 0;
  size_t used = 0;
  FreeSpaceProbe &state = free_space_state();
  if (state.label == nullptr || esp_littlefs_info(state.label, &total, &used) != ESP_OK) {
    // Fail closed: an unreadable free-space value must not disable min_free_bytes.
    if (state.failures++ % PROBE_LOG_EVERY == 0) {
      ESP_LOGW(TAG, "free space could not be read - refusing to grow the database");
    }
    return 0;
  }
  // A working probe clears the failure count, so a new outage warns at once
  // instead of after PROBE_LOG_EVERY more failures.
  state.failures = 0;
  return static_cast<uint64_t>(total) - static_cast<uint64_t>(used);
}

// One scheduled sample was skipped, so the counters and the errors sensor report it like
// any other dropped record instead of hiding the loss.
void TsdbComponent::note_dropped_() {
  this->dropped_++;
  this->publish_errors_();
}

void TsdbComponent::update() {
  this->consume_sync_result_();
  // The sync task holds the engine lock for the whole fold; touching the database here
  // would park the main loop behind it, so this interval's sample is dropped and counted.
  if (this->sync_in_flight_()) {
    this->note_dropped_();
    return;
  }
  if (this->db_ == nullptr) {
    if (this->db_lost_) {
      this->note_dropped_();
    }
    this->reopen_if_lost_();
    return;
  }
  this->handle_requests_();
  // A requested flush can fail and drop the handle, and it may have just started a fold
  // that holds the engine lock; leave the rest of this pass to the next interval.
  if (this->db_ == nullptr || this->sync_in_flight_()) {
    this->note_dropped_();
    return;
  }
  this->write_record_();
  // A write that failed dropped the handle; it already counted the write error, and the
  // aggregates and stats have no database to read until a later interval reopens it.
  if (this->db_ == nullptr)
    return;

  const uint32_t now = App.get_loop_component_start_time();
#ifdef USE_TSDB_AGGREGATES
  if (this->aggregate_interval_ > 0 &&
      (this->last_aggregate_ == 0 || now - this->last_aggregate_ >= this->aggregate_interval_)) {
    this->last_aggregate_ = now;
    this->publish_aggregates_();
  }
#endif
  if (this->last_stats_ == 0 || now - this->last_stats_ >= STATS_INTERVAL_MS) {
    this->last_stats_ = now;
    this->publish_stats_();
  }
  // Requested last: the fold keeps the engine lock for seconds on the sync task, so the
  // diagnostics above still run on every pass, even with a zero sync_interval.
  this->flush_(false);
}

void TsdbComponent::handle_requests_() {
  auto *db = static_cast<tsdb_t *>(this->db_);

  if (this->clear_requested_) {
    this->clear_requested_ = false;
    const esp_err_t err = tsdb_clear_h(db);
    // The engine ignores the error of the fold that persists the cleared header, so the
    // disk may still describe the old history. Leave the recovery copies alone until it
    // really is empty, or a reboot would mix the two states.
    tsdb_header_t persisted = {};
    const bool durable = err == ESP_OK && this->read_disk_header_(&persisted) &&
                         persisted.magic == static_cast<uint32_t>(TSDB_MAGIC) && persisted.total_records == 0;
    if (!durable) {
      this->write_errors_++;
      this->publish_errors_();
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s: clearing the history failed: %s", this->file_.c_str(), esp_err_to_name(err));
      } else {
        ESP_LOGE(TAG, "%s: the cleared header did not reach the file - keeping the recovery copies",
                 this->file_.c_str());
      }
      this->publish_log_("clearing the history did not persist");
      this->status_set_warning(LOG_STR("clearing the history did not persist"));
    } else {
      this->records_ = 0;
      this->writes_ = 0;
      // The guard and the uptime fallback follow the deleted history, so a corrected
      // clock would otherwise be refused until it passes the old newest record.
      this->last_timestamp_ = 0;
      this->dropped_backwards_ = 0;
      this->uptime_offset_ = 1 - static_cast<uint32_t>(::time(nullptr));
      this->dirty_ = false;
      // The durable header slots still hold the cleared history and win an open, so
      // they have to go before the clear counts as done.
      if (this->remove_sidecars_()) {
        ESP_LOGW(TAG, "%s: history cleared on request", this->file_.c_str());
        this->publish_log_("history cleared");
      } else {
        this->write_errors_++;
        this->publish_errors_();
        ESP_LOGE(TAG, "%s: history cleared, but its recovery copies could not be removed - a reboot may restore it",
                 this->file_.c_str());
        this->publish_log_("history cleared, its recovery copies remain");
        this->status_set_warning(LOG_STR("clearing the recovery copies failed"));
      }
    }
  }

  if (this->flush_requested_) {
    this->flush_requested_ = false;
    if (!this->dirty_) {
      // Nothing pending: the file already holds every record.
      ESP_LOGI(TAG, "%s: flushed on request", this->file_.c_str());
      this->publish_log_("history flushed");
    } else {
      // The fold runs on the sync task; its outcome is reported when it finishes.
      this->report_flush_ = true;
      this->flush_(true);
    }
  }

  // A requested flush that failed dropped the handle; a running fold holds the engine
  // lock, so the dump request stays queued for the next pass.
  if (this->db_ == nullptr || this->sync_in_flight_())
    return;

#ifdef USE_TSDB_DUMP
  if (this->dump_requested_ > 0) {
    const uint32_t rows = this->dump_requested_;
    this->dump_requested_ = 0;
    this->dump_csv_(rows);
  }
#endif
}

bool TsdbComponent::resolve_timestamp_(uint32_t *timestamp, const LogString *&degraded) {
  // Any synced time platform also sets the system clock, so ::time() is a usable fallback.
  uint32_t value = static_cast<uint32_t>(::time(nullptr));
  bool have_time = tsdbmath::is_valid_timestamp(value);
#ifdef USE_TIME
  if (this->time_ != nullptr) {
    const ESPTime now = this->time_->now();
    if (now.is_valid()) {
      value = static_cast<uint32_t>(now.timestamp);
      have_time = true;
    }
  }
#endif
  if (have_time) {
    this->warned_time_ = false;
  } else if (this->require_time_) {
    if (!this->warned_time_) {
      this->warned_time_ = true;
      ESP_LOGW(TAG, "%s: no valid time yet - no record written", this->file_.c_str());
    }
    this->dropped_++;
    this->publish_errors_();
    this->status_set_warning(LOG_STR("no valid time yet"));
    return false;
  } else {
    // Without a clock the value is seconds since boot, so the offset continues the series.
    value += this->uptime_offset_;
    degraded = LOG_STR("storing uptime timestamps");
    if (!this->warned_time_) {
      this->warned_time_ = true;
      ESP_LOGW(TAG, "%s: no valid time yet - storing an uptime timestamp", this->file_.c_str());
    }
  }
  // Records are ordered by timestamp, so refuse one that sorts before the newest stored.
  if (this->last_timestamp_ != 0 && value < this->last_timestamp_) {
    this->dropped_++;
    this->publish_errors_();
    this->dropped_backwards_++;
    if (this->dropped_backwards_ == DROP_LOG_EVERY) {
      ESP_LOGE(TAG, "%s: %" PRIu32 " records dropped, newest stored %" PRIu32 " s ahead - recreate the history",
               this->file_.c_str(), this->dropped_backwards_, this->last_timestamp_ - value);
    } else if (this->dropped_backwards_ == 1 || this->dropped_backwards_ % DROP_LOG_EVERY == 0) {
      ESP_LOGW(TAG, "%s: time is %" PRIu32 " s earlier than the last stored record - no record written",
               this->file_.c_str(), this->last_timestamp_ - value);
    } else {
      ESP_LOGD(TAG, "%s: time is %" PRIu32 " s earlier than the last stored record - no record written",
               this->file_.c_str(), this->last_timestamp_ - value);
    }
    this->status_set_warning(LOG_STR("clock moved back"));
    return false;
  }
  *timestamp = value;
  return true;
}

bool TsdbComponent::collect_values_(const LogString *&degraded) {
  this->values_.clear();
  for (TsdbColumn &column : this->columns_) {
    const bool has_value =
        column.source != nullptr && column.source->has_state() && !tsdbmath::is_missing(column.source->state);
    if (has_value) {
      column.hold_warned = false;
      column.sentinel_warned = false;  // so a later outage logs again
      bool saturated = false;
      column.held = tsdbmath::encode(column.source->state, column.scale, column.offset, &saturated);
      if (saturated) {
        this->saturated_++;
        // Per column, so the first clamp of every column is named even when another one clamps on every record.
        if (!column.saturated_warned) {
          column.saturated_warned = true;
          ESP_LOGW(TAG, "%s: %s out of range for scale_factor %.6g - clamped, raise it?", this->file_.c_str(),
                   column.name.c_str(), column.scale);
        }
        if (degraded == nullptr)
          degraded = LOG_STR("a value was clamped");
        this->publish_errors_();  // a clamped value is one of the anomalies reported
      } else {
        column.saturated_warned = false;  // a value that fits re-arms the log
      }
      this->values_.push_back(column.held);
      continue;
    }
    if (this->missing_policy_ == tsdbmath::MISSING_HOLD && column.held != tsdbmath::RAW_UNKNOWN) {
      this->held_++;
      if (!column.hold_warned) {
        column.hold_warned = true;
        ESP_LOGW(TAG, "%s: %s has no value - holding the last value", this->file_.c_str(), column.name.c_str());
      }
      if (degraded == nullptr)
        degraded = LOG_STR("a column holds its last value");
      this->publish_errors_();
      this->values_.push_back(column.held);
      continue;
    }
    if (this->missing_policy_ == tsdbmath::MISSING_SENTINEL) {
      this->sentinel_++;
      if (!column.sentinel_warned) {
        column.sentinel_warned = true;
        ESP_LOGW(TAG, "%s: %s has no value - storing an unknown value", this->file_.c_str(), column.name.c_str());
      }
      if (degraded == nullptr)
        degraded = LOG_STR("a column is stored as unknown");
      this->publish_errors_();  // an unknown value is one of the anomalies reported
      this->values_.push_back(tsdbmath::RAW_UNKNOWN);
      continue;
    }
    this->dropped_++;
    this->dropped_missing_++;
    if (this->dropped_missing_ == 1 || this->dropped_missing_ % DROP_LOG_EVERY == 0) {
      ESP_LOGW(TAG, "%s: %s has no value - record dropped", this->file_.c_str(), column.name.c_str());
    } else {
      ESP_LOGD(TAG, "%s: %s has no value - record dropped", this->file_.c_str(), column.name.c_str());
    }
    this->status_set_warning(LOG_STR("a column has no value"));
    this->publish_errors_();
    return false;
  }
  return true;
}

bool TsdbComponent::write_record_() {
  uint32_t timestamp = 0;
  // The first substitution of a record decides its warning reason: the timestamp before the values.
  const LogString *degraded = nullptr;
  if (!this->resolve_timestamp_(&timestamp, degraded))
    return false;
  if (!this->collect_values_(degraded))
    return false;

  const esp_err_t err = tsdb_write_h(static_cast<tsdb_t *>(this->db_), timestamp, this->values_.data());
  if (err != ESP_OK) {
    this->write_errors_++;
    ESP_LOGE(TAG, "%s: writing a record failed: %s", this->file_.c_str(), esp_err_to_name(err));
    this->status_set_warning(LOG_STR("writing a record failed"));
    // The engine closed the handle; recover instead of failing every write.
    if (!tsdb_is_initialized_h(static_cast<tsdb_t *>(this->db_)))
      this->invalidate_database_(LOG_STR("a write failed and closed the database"));
    this->publish_errors_();
    return false;
  }
  this->last_timestamp_ = timestamp;
  this->dropped_backwards_ = 0;
  this->dirty_ = true;
  // The record block is in the file, but its durable header slot write can fail without
  // the engine reporting it; a reboot would then drop the record from the history.
  const bool durable = this->sidecar_advanced_();
  if (durable) {
    this->writes_++;
    if (this->warned_time_)
      this->uptime_timestamps_++;
    // Only a clean record clears the warning: a held, sentinel or clamped value is not healthy.
    if (degraded != nullptr) {
      this->status_set_warning(degraded);
    } else if (!this->sync_failed_) {
      this->status_clear_warning();
    }
    return true;
  }
  this->write_errors_++;
  ESP_LOGE(TAG, "%s: the record's durable header write failed - a reboot may drop it", this->file_.c_str());
  this->status_set_warning(LOG_STR("the durable header write failed"));
  this->publish_errors_();
  return false;
}

// The diagnostics sensor counts every anomaly - failed writes/syncs, dropped
// records, clamped values, substituted stale ones and values stored as unknown
// - so data loss or corruption is visible.
void TsdbComponent::publish_errors_() {
#ifdef USE_SENSOR
  PUBLISH_STATE(this->errors_sensor_,
                this->write_errors_ + this->dropped_ + this->saturated_ + this->held_ + this->sentinel_);
#endif
}

void TsdbComponent::flush_(bool force) {
  if (this->db_ == nullptr || !this->dirty_ || this->sync_in_flight_())
    return;
  const uint32_t now = App.get_loop_component_start_time();
  if (!force && this->last_sync_ != 0 && this->sync_interval_ > 0 && now - this->last_sync_ < this->sync_interval_)
    return;
  this->last_sync_ = now == 0 ? 1 : now;
  if (!this->sync_task_.is_created()) {
    this->sync_now_();
    return;
  }
  // The fold rewrites the whole file (seconds on a large history); leave it to the task.
  this->sync_state_.store(static_cast<uint8_t>(SyncState::SYNC_STATE_RUNNING), std::memory_order_release);
  xTaskNotify(this->sync_task_.get_handle(), 1, eSetValueWithOverwrite);
}

// Folds the sidecar header into the file, on the main loop (no task) or at shutdown.
void TsdbComponent::sync_now_() {
  auto *db = static_cast<tsdb_t *>(this->db_);
  // A later successful write must not clear the warning raised here: only a sync that
  // works may.
  const esp_err_t err = tsdb_sync_h(db);
  // The engine ignores the error of the header fold, so a sync that "succeeded" can
  // still leave the old header in the file: confirm what reached the disk.
  if (err == ESP_OK && this->folded_header_matches_()) {
    this->finish_sync_(SyncState::SYNC_STATE_OK);
    return;
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "%s: sync failed: %s", this->file_.c_str(), esp_err_to_name(err));
  } else {
    ESP_LOGE(TAG, "%s: sync did not persist the header", this->file_.c_str());
  }
  this->finish_sync_(tsdb_is_initialized_h(db) ? SyncState::SYNC_STATE_FAILED : SyncState::SYNC_STATE_CLOSED);
}

void TsdbComponent::sync_task_fn(void *arg) {
  auto *self = static_cast<TsdbComponent *>(arg);
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    auto *db = static_cast<tsdb_t *>(self->db_);
    esp_err_t err = ESP_ERR_INVALID_STATE;
    if (db != nullptr)
      err = tsdb_sync_h(db);
    SyncState result = SyncState::SYNC_STATE_CLOSED;
    if (err == ESP_OK) {
      // Same check as the inline path: the engine does not report a failed fold.
      result = self->folded_header_matches_() ? SyncState::SYNC_STATE_OK : SyncState::SYNC_STATE_FAILED;
    } else if (db != nullptr && tsdb_is_initialized_h(db)) {
      result = SyncState::SYNC_STATE_FAILED;
    }
    self->sync_state_.store(static_cast<uint8_t>(result), std::memory_order_release);
  }
}

// Consumes the sync task's outcome on the main loop; true when one was pending.
bool TsdbComponent::consume_sync_result_() {
  const auto result = static_cast<SyncState>(this->sync_state_.load(std::memory_order_acquire));
  if (result == SyncState::SYNC_STATE_IDLE || result == SyncState::SYNC_STATE_RUNNING)
    return false;
  this->sync_state_.store(static_cast<uint8_t>(SyncState::SYNC_STATE_IDLE), std::memory_order_relaxed);
  if (result != SyncState::SYNC_STATE_OK)
    ESP_LOGE(TAG, "%s: sync failed", this->file_.c_str());
  this->finish_sync_(result);
  return true;
}

// Applies a sync outcome on the main loop, for both the inline and the task path.
void TsdbComponent::finish_sync_(SyncState result) {
  if (result == SyncState::SYNC_STATE_OK) {
    this->sync_failed_ = false;
    this->dirty_ = false;
    ESP_LOGD(TAG, "%s: synced after %" PRIu32 " writes", this->file_.c_str(), this->writes_);
    if (this->report_flush_) {
      this->report_flush_ = false;
      ESP_LOGI(TAG, "%s: flushed on request", this->file_.c_str());
      this->publish_log_("history flushed");
    }
    return;
  }
  this->report_flush_ = false;
  this->write_errors_++;
  this->sync_failed_ = true;
  this->status_set_warning(LOG_STR("syncing the history failed"));
  this->publish_errors_();
  if (result == SyncState::SYNC_STATE_CLOSED)
    this->invalidate_database_(LOG_STR("a sync failed and closed the database"));
}

bool TsdbComponent::wait_for_sync_() {
  const uint32_t started = millis();
  while (this->sync_in_flight_() && !this->consume_sync_result_()) {
    if (millis() - started > SHUTDOWN_SYNC_TIMEOUT_MS)
      return false;
    delay(10);
  }
  return true;
}

void TsdbComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "tsdb:\n"
                "  file: %s/%s\n"
                "  partition: %s, %" PRIu32 " KB, format on first boot: %s, recreate on schema change: %s\n"
                "  budget: max file %" PRIu32 " KB, min free %" PRIu32 " KB, index stride %" PRIu32 "\n"
                "  columns: %" PRIu32,
                this->mount_point_.c_str(), this->file_.c_str(), this->partition_label_.c_str(),
                this->partition_size_ / 1024, YESNO(this->format_on_first_boot_),
                YESNO(this->recreate_on_schema_change_), this->max_file_size_ / 1024, this->min_free_bytes_ / 1024,
                this->index_stride_, static_cast<uint32_t>(this->columns_.size()));
  for (const TsdbColumn &column : this->columns_) {
    ESP_LOGCONFIG(TAG, "    %s: scale %.6g, offset %.6g", column.name.c_str(), column.scale, column.offset);
  }
  ESP_LOGCONFIG(TAG,
                "  write interval: %" PRIu32 " ms, sync interval: %" PRIu32 " ms\n"
                "  buffer pool: %" PRIu32 " bytes (page size: %" PRIu32 "), memory: %s, paged allocation: %s\n"
                "  time required: %s, missing values: %s",
                this->get_update_interval(), this->sync_interval_, this->buffer_pool_size_, this->page_size_,
                LOG_STR_ARG(memory_mode_to_string(this->memory_mode_)), YESNO(this->paged_allocation_),
                YESNO(this->require_time_), LOG_STR_ARG(missing_policy_to_string(this->missing_policy_)));
#ifdef USE_TSDB_AGGREGATES
  ESP_LOGCONFIG(TAG, "  aggregates: window %" PRIu32 " s, interval %" PRIu32 " ms", this->aggregate_window_,
                this->aggregate_interval_);
#endif
#ifdef USE_TSDB_DUMP
  ESP_LOGCONFIG(TAG, "  dump rows: %" PRIu32, this->dump_rows_);
#endif
  ESP_LOGCONFIG(TAG,
                "  writes: %" PRIu32 ", errors: %" PRIu32 ", dropped: %" PRIu32 ", clamped: %" PRIu32 ", held: %" PRIu32
                ", sentinel: %" PRIu32 ", uptime: %" PRIu32,
                this->writes_, this->write_errors_, this->dropped_, this->saturated_, this->held_, this->sentinel_,
                this->uptime_timestamps_);
  if (this->reopen_errors_ > 0) {
    ESP_LOGCONFIG(TAG, "  failed reopens: %" PRIu32, this->reopen_errors_);
  }
}

#ifdef USE_TSDB_AGGREGATES
void TsdbComponent::publish_aggregates_() {
  if (this->time_ == nullptr || this->aggregate_window_ == 0)
    return;
  // Nothing to publish: skip scanning the window when no aggregate sensor is configured.
  const bool has_aggregates =
      std::any_of(this->aggregates_.begin(), this->aggregates_.end(), [](const TsdbAggregateSensors &sensors) {
        return sensors.average != nullptr || sensors.min != nullptr || sensors.max != nullptr;
      });
  if (!has_aggregates)
    return;
  const ESPTime now = this->time_->now();
  if (!now.is_valid())
    return;
  const uint32_t end = static_cast<uint32_t>(now.timestamp);
  const uint32_t start = end > this->aggregate_window_ ? end - this->aggregate_window_ : 1;

  // The engine aggregates the raw codes: it counts the unknown sentinel, and a
  // negative scale reverses the order of MIN/MAX. Read the window once and
  // aggregate the decoded values instead.
  std::array<tsdbmath::Aggregate, TSDB_COLUMN_COUNT> windows{};

  auto *db = static_cast<tsdb_t *>(this->db_);
  // Static, like the dump query: the query handle is large and the database lock
  // keeps the two from running at once.
  static tsdb_query_t query = {};
  if (tsdb_query_init_h(db, &query, start, end, nullptr, 0) != ESP_OK) {
    ESP_LOGW(TAG, "%s: reading the aggregate window failed", this->file_.c_str());
    return;
  }
  this->values_.resize(this->columns_.size());
  uint32_t timestamp = 0;
  while (tsdb_query_next(&query, &timestamp, this->values_.data()) == ESP_OK) {
    for (size_t index = 0; index < this->columns_.size(); index++) {
      windows[index].add_raw(this->values_[index], this->columns_[index].scale, this->columns_[index].offset);
    }
  }
  tsdb_query_close(&query);

  for (size_t index = 0; index < this->columns_.size(); index++) {
    const TsdbAggregateSensors &sensors = this->aggregates_[index];
    // A window with no valid sample leaves the aggregate unknown.
    PUBLISH_STATE(sensors.average, static_cast<float>(windows[index].average()));
    PUBLISH_STATE(sensors.min, static_cast<float>(windows[index].min()));
    PUBLISH_STATE(sensors.max, static_cast<float>(windows[index].max()));
  }
}
#endif  // USE_TSDB_AGGREGATES

// Wall-clock seconds, or 0 when no clock is synced (record ages are then unknown).
uint32_t TsdbComponent::wall_clock_() const {
#ifdef USE_TIME
  if (this->time_ != nullptr) {
    const ESPTime now = this->time_->now();
    if (now.is_valid())
      return static_cast<uint32_t>(now.timestamp);
  }
#endif
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
#ifdef USE_SENSOR
  PUBLISH_STATE(this->records_sensor_, stats.total_records);
  PUBLISH_STATE(this->evictions_sensor_, stats.total_evictions);

  // Ages rather than timestamps: a uint32 epoch in a float state loses about
  // two minutes, which is more than a retention window's usable resolution.
  const uint32_t now = this->wall_clock_();
  const bool ages_known = now != 0 && stats.total_records > 0 && tsdbmath::is_valid_timestamp(stats.oldest_timestamp);
  const uint32_t oldest_age = ages_known && now > stats.oldest_timestamp ? now - stats.oldest_timestamp : 0;
  const uint32_t newest_age = ages_known && now > stats.newest_timestamp ? now - stats.newest_timestamp : 0;
  PUBLISH_STATE(this->oldest_age_sensor_, ages_known ? static_cast<float>(oldest_age) : NAN);
  PUBLISH_STATE(this->newest_age_sensor_, ages_known ? static_cast<float>(newest_age) : NAN);
  this->publish_errors_();

  // esp_littlefs_info() walks the filesystem, so it only runs when a sensor needs it.
  size_t total = 0;
  size_t used = 0;
  if ((this->used_sensor_ != nullptr || this->free_sensor_ != nullptr) &&
      esp_littlefs_info(this->partition_label_.c_str(), &total, &used) == ESP_OK) {
    PUBLISH_STATE(this->used_sensor_, static_cast<float>(used));
    PUBLISH_STATE(this->free_sensor_, static_cast<float>(total - used));
  }
#endif
  ESP_LOGD(
      TAG, "%s: %" PRIu32 " records, %" PRIu32 " evicted, %" PRIu32 " writes, %" PRIu32 " dropped, %" PRIu32 " errors",
      this->file_.c_str(), this->records_, stats.total_evictions, this->writes_, this->dropped_, this->write_errors_);
}

#ifdef USE_TSDB_DUMP
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

  // A header that a log line truncates would name fewer columns than the rows carry.
  size_t header_bytes = sizeof("timestamp") - 1;
  for (const TsdbColumn &column : this->columns_)
    header_bytes += 1 + column.name.size();
  if (header_bytes >= CSV_HEADER_LIMIT) {
    ESP_LOGE(TAG,
             "%s: csv-dump header needs %" PRIu32 " bytes, a log line holds %" PRIu32
             " - shorten the column names or raise the logger's tx_buffer_size",
             this->file_.c_str(), static_cast<uint32_t>(header_bytes), static_cast<uint32_t>(CSV_HEADER_LIMIT));
    return;
  }
  // Column names in a stack buffer: header and rows stay heap free.
  char header[CSV_HEADER_SIZE];
  int header_length = snprintf(header, sizeof(header), "timestamp");
  for (const auto &column : this->columns_) {
    header_length += snprintf(header + header_length, sizeof(header) - static_cast<size_t>(header_length), ",%s",
                              column.name.c_str());
  }
  // The preamble goes on its own line so the header gets the whole log line, which is
  // what the budget above measured.
  ESP_LOGI(TAG, "%s: csv-dump begin (newest %" PRIu32 " records)", this->file_.c_str(), rows);
  ESP_LOGI(TAG, "%s", header);

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
      ESP_LOGI(TAG, "%s", line);
    }
    count++;
  }
  tsdb_query_close(&query);
  ESP_LOGI(TAG, "%s: csv-dump end (%" PRIu32 " rows)", this->file_.c_str(), count);
}
#endif  // USE_TSDB_DUMP

void TsdbComponent::publish_log_(const char *message) {
#ifdef USE_TEXT_SENSOR
  PUBLISH_STATE(this->log_sensor_, message);
#endif
}

}  // namespace esphome::tsdb

#endif  // USE_ESP_IDF
