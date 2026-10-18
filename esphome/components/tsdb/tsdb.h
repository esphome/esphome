#pragma once

// tsdb hub header; ESP-IDF only (esp_tsdb + LittleFS engine).
#ifdef USE_ESP_IDF

#include <array>
#include <cstdint>

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/string_ref.h"
#include "esphome/components/sensor/sensor.h"
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

#include "esp_err.h"

#include "tsdb_math.h"

namespace esphome::tsdb {

struct TsdbColumn {
  StringRef name;
  sensor::Sensor *source{nullptr};
  double scale{1.0};
  double offset{0.0};
  int16_t held{tsdbmath::RAW_UNKNOWN};
  bool hold_warned{false};
};

#ifdef USE_SENSOR
// Kept apart from TsdbColumn: the sensor platform may set these before the hub adds its columns.
struct TsdbAggregateSensors {
  sensor::Sensor *average{nullptr};
  sensor::Sensor *min{nullptr};
  sensor::Sensor *max{nullptr};
};
#endif

class TsdbComponent : public PollingComponent {
#ifdef USE_SENSOR
  SUB_SENSOR(records)
  SUB_SENSOR(used)
  SUB_SENSOR(free)
  SUB_SENSOR(oldest_age)
  SUB_SENSOR(newest_age)
  SUB_SENSOR(evictions)
  SUB_SENSOR(errors)
#endif

 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  void on_shutdown() override;

  void set_file(const char *file) { this->file_ = StringRef(file); }
  void set_mount_point(const char *mount_point) { this->mount_point_ = StringRef(mount_point); }
  void set_partition_label(const char *label) { this->partition_label_ = StringRef(label); }
  void set_partition_size(uint32_t bytes) { this->partition_size_ = bytes; }
  void set_format_on_first_boot(bool format) { this->format_on_first_boot_ = format; }
  void set_recreate_on_schema_change(bool recreate) { this->recreate_on_schema_change_ = recreate; }
  void set_max_file_size(uint32_t bytes) { this->max_file_size_ = bytes; }
  void set_index_stride(uint32_t stride) { this->index_stride_ = stride; }
  void set_buffer_pool_size(uint32_t bytes) { this->buffer_pool_size_ = bytes; }
  void set_memory_mode(tsdbmath::MemoryMode mode) { this->memory_mode_ = mode; }
  void set_paged_allocation(bool paged) { this->paged_allocation_ = paged; }
  void set_page_size(uint32_t bytes) { this->page_size_ = bytes; }
  void set_sync_interval(uint32_t ms) { this->sync_interval_ = ms; }
  void set_min_free_bytes(uint32_t bytes) { this->min_free_bytes_ = bytes; }
  void set_require_time(bool require) { this->require_time_ = require; }
  void set_missing_policy(tsdbmath::MissingPolicy policy) { this->missing_policy_ = policy; }
  void set_aggregate_window(uint32_t seconds) { this->aggregate_window_ = seconds; }
  void set_aggregate_interval(uint32_t ms) { this->aggregate_interval_ = ms; }
  void set_dump_rows(uint32_t rows) { this->dump_rows_ = rows; }
#ifdef USE_TIME
  void set_time_source(time::RealTimeClock *clock) { this->time_ = clock; }
#endif

  void add_column(const char *name, double scale, double offset);
  void set_column_source(size_t index, sensor::Sensor *source);
#ifdef USE_SENSOR
  void set_column_average_sensor(size_t index, sensor::Sensor *sensor) { this->aggregates_[index].average = sensor; }
  void set_column_min_sensor(size_t index, sensor::Sensor *sensor) { this->aggregates_[index].min = sensor; }
  void set_column_max_sensor(size_t index, sensor::Sensor *sensor) { this->aggregates_[index].max = sensor; }
#endif
#ifdef USE_TEXT_SENSOR
  void set_log_sensor(text_sensor::TextSensor *sensor) { this->log_sensor_ = sensor; }
#endif

  void request_flush() { this->flush_requested_ = true; }
  void request_dump(uint32_t rows = 0) { this->dump_requested_ = rows == 0 ? this->dump_rows_ : rows; }
  void request_clear() { this->clear_requested_ = true; }
  uint32_t get_records() const { return this->records_; }
  bool is_ready() const { return this->db_ != nullptr; }

 protected:
  bool mount_filesystem_();
  void unmount_filesystem_();
  bool open_database_();
  void log_init_failure_();
  void fail_setup_(const LogString *reason);
  void close_database_();
  void handle_requests_();
  void invalidate_database_(const LogString *reason);
  void reopen_if_lost_();
  bool read_saved_index_stride_(uint32_t *stride) const;
  bool write_record_();
  void publish_errors_();
  void flush_(bool force);
  void publish_aggregates_();
  void publish_stats_();
  void dump_csv_(uint32_t rows);
  void publish_log_(const char *message);
  uint32_t wall_clock_() const;
  static uint64_t free_space_probe();

  StringRef file_{"history.tsdb"};
  StringRef mount_point_{"/littlefs"};
  StringRef partition_label_{"littlefs"};
  uint32_t partition_size_{512 * 1024};
  bool format_on_first_boot_{true};
  bool recreate_on_schema_change_{false};
  uint32_t max_file_size_{384 * 1024};
  uint32_t index_stride_{tsdbmath::INDEX_DEFAULT_STRIDE};
  uint32_t buffer_pool_size_{4096};
  tsdbmath::MemoryMode memory_mode_{tsdbmath::MEMORY_AUTO};
  bool paged_allocation_{false};
  uint32_t page_size_{2048};
  uint32_t sync_interval_{60000};
  uint32_t min_free_bytes_{32768};
  bool require_time_{true};
  tsdbmath::MissingPolicy missing_policy_{tsdbmath::MISSING_SKIP};
  uint32_t aggregate_window_{3600};
  uint32_t aggregate_interval_{300000};
  uint32_t dump_rows_{60};
  // Sized by TSDB_COLUMN_COUNT, emitted by the Python config; defines.h covers static analysis.
  StaticVector<TsdbColumn, TSDB_COLUMN_COUNT> columns_;
  StaticVector<int16_t, TSDB_COLUMN_COUNT> values_;
#ifdef USE_TIME
  time::RealTimeClock *time_{nullptr};
#endif
#ifdef USE_SENSOR
  std::array<TsdbAggregateSensors, TSDB_COLUMN_COUNT> aggregates_{};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *log_sensor_{nullptr};
#endif

  void *db_{nullptr};
  bool mounted_{false};
  // A handle esp_tsdb invalidated; reopened on a later interval.
  bool db_lost_{false};
  uint32_t last_reopen_{0};
  uint32_t writes_{0};
  uint32_t write_errors_{0};
  // Apart from write_errors_: a database that never returns is not a write failure.
  uint32_t reopen_errors_{0};
  uint32_t dropped_{0};
  uint32_t dropped_missing_{0};
  uint32_t saturated_{0};
  uint32_t held_{0};
  uint32_t last_sync_{0};
  uint32_t last_timestamp_{0};
  // Added to an uptime timestamp so the series continues after the newest stored record.
  uint32_t uptime_offset_{0};
  bool dirty_{false};
  uint32_t last_aggregate_{0};
  uint32_t last_stats_{0};
  uint32_t records_{0};
  bool flush_requested_{false};
  uint32_t dump_requested_{0};
  bool clear_requested_{false};
  bool warned_time_{false};
  bool warned_backwards_{false};
  // Raised by a failed sync, cleared by the next one, so a successful write cannot.
  bool sync_failed_{false};
  esp_err_t mount_error_{ESP_OK};
  bool format_attempted_{false};
  esp_err_t format_error_{ESP_OK};
};

}  // namespace esphome::tsdb

#endif  // USE_ESP_IDF
