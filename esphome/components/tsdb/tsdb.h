#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/time/real_time_clock.h"

#include "tsdb_math.h"

namespace esphome::tsdb {

/// One logged column: an ESPHome sensor, its raw encoding and its optional
/// aggregates over `aggregate_window`.
struct TsdbColumn {
  std::string name;  ///< column name in the database (informational, stored in the file header)
  sensor::Sensor *source{nullptr};
  double scale{1.0};   ///< raw = round(value * scale + offset)
  double offset{0.0};  ///< see `scale`
  sensor::Sensor *average_sensor{nullptr};
  sensor::Sensor *min_sensor{nullptr};
  sensor::Sensor *max_sensor{nullptr};
  int16_t held{tsdbmath::RAW_UNKNOWN};  ///< last raw value written (`on_missing: hold`)
};

/// Polling hub that appends one row per `update_interval` to a time-series
/// database on a LittleFS partition (engine: esp_tsdb, MIT).
///
/// The component owns the partition (it asks ESPHome for it via
/// `esp32.add_partition()`), mounts it, opens one database handle and writes the
/// current state of every configured `sensor:` as one row. `tsdb_sync_h()` closes
/// and reopens the file on `sync_interval`, which is what publishes the LittleFS
/// directory entry - everything written since the last sync is at risk on a
/// power cut, which is the single most important number of this component.
class TsdbComponent : public PollingComponent {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  void on_shutdown() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // ------------------------------------------------------------------ config
  void set_file(const std::string &file) { this->file_ = file; }
  void set_mount_point(const std::string &mount_point) { this->mount_point_ = mount_point; }
  void set_partition_label(const std::string &label) { this->partition_label_ = label; }
  void set_partition_size(uint32_t bytes) { this->partition_size_ = bytes; }
  void set_format_on_first_boot(bool format) { this->format_on_first_boot_ = format; }
  /// Delete a database the engine refuses to open because its stored column set
  /// differs from `columns`, and start a fresh one instead of failing (see the
  /// README - the stored rows of the old schema are lost, that is the point).
  void set_recreate_on_schema_change(bool recreate) { this->recreate_on_schema_change_ = recreate; }
  void set_max_file_size(uint32_t bytes) { this->max_file_size_ = bytes; }
  void set_index_stride(uint32_t stride) { this->index_stride_ = stride; }
  void set_buffer_pool_size(uint32_t bytes) { this->buffer_pool_size_ = bytes; }
  void set_memory_mode(uint8_t mode) { this->memory_mode_ = mode; }
  void set_paged_allocation(bool paged) { this->paged_allocation_ = paged; }
  void set_page_size(uint32_t bytes) { this->page_size_ = bytes; }
  void set_sync_interval(uint32_t ms) { this->sync_interval_ = ms; }
  void set_min_free_bytes(uint32_t bytes) { this->min_free_bytes_ = bytes; }
  void set_require_time(bool require) { this->require_time_ = require; }
  void set_missing_policy(uint8_t policy) { this->missing_policy_ = policy; }
  void set_aggregate_window(uint32_t seconds) { this->aggregate_window_ = seconds; }
  void set_aggregate_interval(uint32_t ms) { this->aggregate_interval_ = ms; }
  void set_dump_rows(uint32_t rows) { this->dump_rows_ = rows; }
  void set_time_source(time::RealTimeClock *clock) { this->time_ = clock; }

  /// Append a column; the source sensor and the diagnostic sensors follow with
  /// the setters of the same index (that is how `__init__.py` builds them).
  void add_column(const std::string &name, double scale, double offset);
  void set_column_source(size_t index, sensor::Sensor *source);
  void set_column_average_sensor(size_t index, sensor::Sensor *sensor);
  void set_column_min_sensor(size_t index, sensor::Sensor *sensor);
  void set_column_max_sensor(size_t index, sensor::Sensor *sensor);

  void set_records_sensor(sensor::Sensor *sensor) { this->records_sensor_ = sensor; }
  void set_used_sensor(sensor::Sensor *sensor) { this->used_sensor_ = sensor; }
  void set_free_sensor(sensor::Sensor *sensor) { this->free_sensor_ = sensor; }
  void set_oldest_sensor(sensor::Sensor *sensor) { this->oldest_sensor_ = sensor; }
  void set_newest_sensor(sensor::Sensor *sensor) { this->newest_sensor_ = sensor; }
  void set_errors_sensor(sensor::Sensor *sensor) { this->errors_sensor_ = sensor; }
  void set_log_sensor(text_sensor::TextSensor *sensor) { this->log_sensor_ = sensor; }

  // ----------------------------------------------------------------- runtime
  /// Ask for a `tsdb_sync_h()` on the next `update()` (the "flush" button).
  void request_flush() { this->flush_requested_ = true; }
  /// Ask for a CSV dump of the newest `rows` records on the next `update()`
  /// (the "dump" button; `rows == 0` uses `dump_rows`). The window is `rows`
  /// write intervals up to the newest record, so the engine seeks to it instead
  /// of scanning the whole history.
  void request_dump(uint32_t rows = 0) { this->dump_requested_ = rows == 0 ? this->dump_rows_ : rows; }
  /// Ask for `tsdb_clear_h()` on the next `update()` (the "clear" button).
  void request_clear() { this->clear_requested_ = true; }
  /// Number of rows currently in the database (0 until the first statistics pass).
  uint32_t get_records() const { return this->records_; }
  /// `true` once the partition is mounted and the database is open.
  bool is_ready() const { return this->db_ != nullptr; }

 protected:
  bool mount_filesystem_();
  bool open_database_();
  void close_database_();
  void handle_requests_();
  bool write_record_();
  /// Commit the file when `force`, and otherwise once a record was written since
  /// the last commit **and** `sync_interval` has passed.
  void flush_(bool force);
  void publish_aggregates_();
  void publish_stats_();
  void dump_csv_(uint32_t rows);
  void publish_log_(const char *message);
  static uint64_t free_space_probe_();  // the callback shape esp_tsdb requires

  // ------------------------------------------------------------------ config
  std::string file_{"history.tsdb"};
  std::string mount_point_{"/littlefs"};
  std::string partition_label_{"littlefs"};
  uint32_t partition_size_{512 * 1024};
  bool format_on_first_boot_{true};
  bool recreate_on_schema_change_{false};
  uint32_t max_file_size_{384 * 1024};
  uint32_t index_stride_{tsdbmath::INDEX_DEFAULT_STRIDE};
  uint32_t buffer_pool_size_{4096};
  uint8_t memory_mode_{tsdbmath::MEMORY_AUTO};
  bool paged_allocation_{false};
  uint32_t page_size_{2048};
  uint32_t sync_interval_{60000};
  uint32_t min_free_bytes_{32768};
  bool require_time_{true};
  uint8_t missing_policy_{tsdbmath::MISSING_SKIP};
  uint32_t aggregate_window_{3600};
  uint32_t aggregate_interval_{300000};
  uint32_t dump_rows_{60};
  std::vector<TsdbColumn> columns_{};
  time::RealTimeClock *time_{nullptr};

  sensor::Sensor *records_sensor_{nullptr};
  sensor::Sensor *used_sensor_{nullptr};
  sensor::Sensor *free_sensor_{nullptr};
  sensor::Sensor *oldest_sensor_{nullptr};
  sensor::Sensor *newest_sensor_{nullptr};
  sensor::Sensor *errors_sensor_{nullptr};
  text_sensor::TextSensor *log_sensor_{nullptr};

  // ----------------------------------------------------------------- runtime
  void *db_{nullptr};  ///< `tsdb_t *`, kept opaque so this header stays esp_tsdb-free
  bool mounted_{false};
  uint32_t writes_{0};
  uint32_t write_errors_{0};
  uint32_t dropped_{0};
  uint32_t last_sync_{0};
  bool dirty_{false};  ///< a record was written since the last successful sync
  uint32_t last_aggregate_{0};
  uint32_t last_stats_{0};
  uint32_t records_{0};
  bool flush_requested_{false};
  uint32_t dump_requested_{0};
  bool clear_requested_{false};
  bool warned_time_{false};
};

}  // namespace esphome::tsdb
