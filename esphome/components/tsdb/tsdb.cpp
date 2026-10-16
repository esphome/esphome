#include "tsdb.h"

#include <cinttypes>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <ctime>

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
// Every Nth dropped record logs at WARN, the rest at DEBUG.
static constexpr uint32_t DROP_LOG_EVERY = 10;

static const char *const INIT_STAGE_CONFIG = "the configuration";
static const char *const INIT_STAGE_MOUNT = "mounting the LittleFS partition";
static const char *const INIT_STAGE_OPEN = "opening the database";

// esp_tsdb free-space callback takes no context; the mounted label lives here.
static const char *g_free_space_label = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static_assert(tsdbmath::BLOCK_SIZE == TSDB_BLOCK_SIZE, "esp_tsdb changed TSDB_BLOCK_SIZE");
static_assert(tsdbmath::BLOCK_HEADER_SIZE == TSDB_BLOCK_HEADER_SIZE, "esp_tsdb changed the block header");
static_assert(tsdbmath::records_per_block(4) == (TSDB_BLOCK_SIZE - TSDB_BLOCK_HEADER_SIZE) / (4 + 4 * 2),
              "esp_tsdb changed the record geometry");
static_assert(tsdbmath::max_records_for_bytes(3072, 4) == TSDB_CALC_MAX_RECORDS(3072, 4),
              "esp_tsdb changed TSDB_CALC_MAX_RECORDS()");
static_assert(sizeof(tsdb_header_t) == tsdbmath::HEADER_BYTES, "esp_tsdb changed sizeof(tsdb_header_t)");

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

void TsdbComponent::add_column(const std::string &name, double scale, double offset) {
  TsdbColumn column;
  column.name = name;
  column.scale = scale;
  column.offset = offset;
  this->columns_.push_back(column);
}

void TsdbComponent::set_column_source(size_t index, sensor::Sensor *source) { this->columns_[index].source = source; }

void TsdbComponent::setup() {
  if (this->columns_.empty()) {
    ESP_LOGE(TAG, "tsdb: no columns configured");
    this->failed_stage_ = INIT_STAGE_CONFIG;
    this->log_init_failure_(this->failed_stage_);
    this->mark_failed(LOG_STR("no columns configured"));
    return;
  }
  if (!this->mount_filesystem_()) {
    this->failed_stage_ = INIT_STAGE_MOUNT;
    this->log_init_failure_(this->failed_stage_);
    this->mark_failed(LOG_STR("mounting the LittleFS partition"));
    return;
  }
  if (!this->open_database_()) {
    this->failed_stage_ = INIT_STAGE_OPEN;
    this->log_init_failure_(this->failed_stage_);
    this->mark_failed(LOG_STR("opening the database"));
    return;
  }
  ESP_LOGI(TAG, "%s: history opened", this->file_.c_str());
}

void TsdbComponent::on_shutdown() {
  if (this->db_ == nullptr)
    return;
  this->flush_(true);
  ESP_LOGI(TAG, "%s: closing on shutdown", this->file_.c_str());
  this->close_database_();
}

bool TsdbComponent::mount_filesystem_() {
  const esp_partition_t *partition = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, static_cast<esp_partition_subtype_t>(LITTLEFS_SUBTYPE), this->partition_label_.c_str());
  if (partition == nullptr) {
    ESP_LOGE(TAG, "%s: no data/littlefs partition '%s' - flash the partition table over USB", this->file_.c_str(),
             this->partition_label_.c_str());
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
  this->mount_error_ = err;
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

static size_t file_size_of(const std::string &path) {
  FILE *file = fopen(path.c_str(), "rb");
  if (file == nullptr)
    return 0;
  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return 0;
  }
  const int64_t size = static_cast<int64_t>(ftell(file));
  fclose(file);
  return size > 0 ? static_cast<size_t>(size) : 0;
}

void TsdbComponent::log_init_failure_(const char *stage) {
  const std::string path = this->mount_point_ + "/" + this->file_;
  ESP_LOGE(TAG, "history is NOT available: setup failed at %s", stage);
  const esp_partition_t *partition = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, static_cast<esp_partition_subtype_t>(LITTLEFS_SUBTYPE), this->partition_label_.c_str());
  if (partition == nullptr) {
    ESP_LOGE(TAG, "  partition '%s': NOT FOUND", this->partition_label_.c_str());
  } else {
    ESP_LOGE(TAG, "  partition '%s': at 0x%08" PRIx32 ", %" PRIu32 " bytes", partition->label,
             static_cast<uint32_t>(partition->address), static_cast<uint32_t>(partition->size));
  }
  ESP_LOGE(TAG, "  mount: %s, %s: %s", esp_err_to_name(this->mount_error_), path.c_str(),
           database_file_exists(path) ? LOG_STR_LITERAL("present") : LOG_STR_LITERAL("missing"));
  if (database_file_exists(path))
    ESP_LOGE(TAG, "    %" PRIu32 " bytes", static_cast<uint32_t>(file_size_of(path)));
  ESP_LOGE(TAG, "  heap: %" PRIu32 " bytes internal free",
           static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL)));
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
  config.min_free_bytes = this->min_free_bytes_;
  config.free_space_cb = this->min_free_bytes_ > 0 ? free_space_probe : nullptr;

  this->db_ = tsdb_open(&config);
  if (this->db_ == nullptr) {
    ESP_LOGE(TAG, "%s: esp_tsdb refused to open %s (%" PRIu32 " columns, up to %" PRIu32 " records)",
             this->file_.c_str(), path.c_str(), static_cast<uint32_t>(names.size()), max_records);
    return false;
  }

  ESP_LOGI(TAG, "%s: %" PRIu32 " columns, %" PRIu32 " records at %s", this->file_.c_str(),
           static_cast<uint32_t>(names.size()), max_records, path.c_str());
  return true;
}

void TsdbComponent::close_database_() {
  if (this->db_ == nullptr)
    return;
  tsdb_close_h(static_cast<tsdb_t *>(this->db_));
  this->db_ = nullptr;
}

uint64_t TsdbComponent::free_space_probe() {
  if (g_free_space_label == nullptr)
    return UINT64_MAX;
  size_t total = 0;
  size_t used = 0;
  if (esp_littlefs_info(g_free_space_label, &total, &used) != ESP_OK)
    return UINT64_MAX;
  return static_cast<uint64_t>(total) - static_cast<uint64_t>(used);
}

void TsdbComponent::update() {
  if (this->db_ == nullptr)
    return;
  this->write_record_();
  this->flush_(false);
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
      ESP_LOGW(TAG, "%s: no valid time yet - no record written", this->file_.c_str());
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
    this->dropped_++;
    if (this->dropped_ == 1 || this->dropped_ % DROP_LOG_EVERY == 0) {
      ESP_LOGW(TAG, "%s: %s has no value - record dropped", this->file_.c_str(), column.name.c_str());
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
  this->dirty_ = true;
  return true;
}

void TsdbComponent::flush_(bool force) {
  if (this->db_ == nullptr)
    return;
  const uint32_t now = millis();
  if (!force && !this->dirty_)
    return;
  if (!force && this->last_sync_ != 0 && this->sync_interval_ > 0 && now - this->last_sync_ < this->sync_interval_)
    return;
  this->last_sync_ = now == 0 ? 1 : now;

  const esp_err_t err = tsdb_sync_h(static_cast<tsdb_t *>(this->db_));
  if (err != ESP_OK) {
    this->write_errors_++;
    ESP_LOGE(TAG, "%s: sync failed: %s", this->file_.c_str(), esp_err_to_name(err));
    if (this->errors_sensor_ != nullptr)
      this->errors_sensor_->publish_state(this->write_errors_);
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
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  the database is NOT available: setup failed at %s",
             this->failed_stage_ != nullptr ? this->failed_stage_ : LOG_STR_LITERAL("an unknown stage"));
  }
}

}  // namespace esphome::tsdb
