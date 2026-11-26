#pragma once

#ifdef USE_ESP_IDF

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/static_task.h"
#include "esphome/core/string_ref.h"
#include "esphome/components/sensor/sensor.h"
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

#include "esp_err.h"
#include "esp_tsdb.h"

#include "tsdb_math.h"

namespace esphome::tsdb {

// mount_point is at most 32 bytes and file at most 64, plus the slash, a slot suffix
// ("/.d/h0") and the terminator. The schema enforces the byte limits.
static constexpr size_t PATH_BUFFER_SIZE = 128;
using PathBuffer = std::array<char, PATH_BUFFER_SIZE>;

// Result of the header inspection done before tsdb_open(), which unlinks a file it
// cannot read before any schema check.
enum class HistoryState : uint8_t {
  HISTORY_STATE_MISSING = 0,
  HISTORY_STATE_READABLE = 1,
  HISTORY_STATE_UNREADABLE = 2,
};

// The one flag shared with the sync task: the main loop sets RUNNING, the task leaves the
// outcome, and the main loop consumes it back to IDLE. The main loop leaves the database
// alone in every state but IDLE.
enum class SyncState : uint8_t {
  SYNC_STATE_IDLE = 0,
  SYNC_STATE_RUNNING = 1,
  SYNC_STATE_OK = 2,
  SYNC_STATE_FAILED = 3,
  SYNC_STATE_CLOSED = 4,
};

struct TsdbColumn {
  StringRef name;
  sensor::Sensor *source{nullptr};
  double scale{1.0};
  double offset{0.0};
  int16_t held{tsdbmath::RAW_UNKNOWN};
  bool hold_warned{false};
  bool sentinel_warned{false};
  bool saturated_warned{false};
};

#ifdef USE_TSDB_AGGREGATES
// Kept apart from TsdbColumn: the sensor platform may set these before the hub adds its columns.
struct TsdbAggregateSensors {
  sensor::Sensor *average{nullptr};
  sensor::Sensor *min{nullptr};
  sensor::Sensor *max{nullptr};
};
#endif

class TsdbComponent final : public PollingComponent {
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
  TsdbComponent(const char *file, const char *mount_point, const char *partition_label)
      : file_(file), mount_point_(mount_point), partition_label_(partition_label) {}

  void setup() override;
  void update() override;
  void dump_config() override;
  void on_shutdown() override;

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
#ifdef USE_TSDB_AGGREGATES
  void set_aggregate_window(uint32_t seconds) { this->aggregate_window_ = seconds; }
  void set_aggregate_interval(uint32_t ms) { this->aggregate_interval_ = ms; }
#endif
#ifdef USE_TSDB_DUMP
  void set_dump_rows(uint32_t rows) { this->dump_rows_ = rows; }
#endif
#ifdef USE_TIME
  void set_time_source(time::RealTimeClock *clock) { this->time_ = clock; }
#endif

  void add_column(const char *name, double scale, double offset);
  void set_column_source(size_t index, sensor::Sensor *source);
#ifdef USE_TSDB_AGGREGATES
  void set_column_average_sensor(size_t index, sensor::Sensor *sensor) { this->aggregates_[index].average = sensor; }
  void set_column_min_sensor(size_t index, sensor::Sensor *sensor) { this->aggregates_[index].min = sensor; }
  void set_column_max_sensor(size_t index, sensor::Sensor *sensor) { this->aggregates_[index].max = sensor; }
#endif
#ifdef USE_TEXT_SENSOR
  void set_log_sensor(text_sensor::TextSensor *sensor) { this->log_sensor_ = sensor; }
#endif

  void request_flush() { this->flush_requested_ = true; }
#ifdef USE_TSDB_DUMP
  void request_dump(uint32_t rows = 0) { this->dump_requested_ = rows == 0 ? this->dump_rows_ : rows; }
#endif
  void request_clear() { this->clear_requested_ = true; }
  uint32_t get_records() const { return this->records_; }
  bool is_ready() const { return this->db_ != nullptr; }

 protected:
  bool mount_filesystem_();
  void unmount_filesystem_();
  bool open_database_();
  HistoryState inspect_history_(const char *path, const LogString **reason) const;
  void log_init_failure_();
  void fail_setup_(const LogString *reason);
  void close_database_();
  void handle_requests_();
  void note_dropped_();
  void invalidate_database_(const LogString *reason);
  void reopen_if_lost_();
  bool read_saved_index_stride_(uint32_t *stride) const;
  bool resolve_timestamp_(uint32_t *timestamp, const LogString *&degraded);
  bool collect_values_(const LogString *&degraded);
  bool write_record_();
  void publish_errors_();
  void flush_(bool force);
  void sync_now_();
  bool consume_sync_result_();
  void finish_sync_(SyncState result);
  bool wait_for_sync_();
  bool sync_in_flight_() const {
    return this->sync_state_.load(std::memory_order_acquire) != static_cast<uint8_t>(SyncState::SYNC_STATE_IDLE);
  }
  static void sync_task_fn(void *arg);
  PathBuffer format_path_() const;
  bool remove_sidecars_();
  bool sidecar_advanced_();
  uint32_t newest_sidecar_seq_on_disk_() const;
  bool read_disk_header_(tsdb_header_t *header) const;
  bool folded_header_matches_();
#ifdef USE_TSDB_AGGREGATES
  void publish_aggregates_();
#endif
  void publish_stats_();
#ifdef USE_TSDB_DUMP
  void dump_csv_(uint32_t rows);
#endif
  void publish_log_(const char *message);
  uint32_t wall_clock_() const;
  static uint64_t free_space_probe();

  // Sized by TSDB_COLUMN_COUNT, emitted by the Python config; defines.h covers static analysis.
  StaticVector<TsdbColumn, TSDB_COLUMN_COUNT> columns_;
  StaticVector<int16_t, TSDB_COLUMN_COUNT> values_;
#ifdef USE_TSDB_AGGREGATES
  std::array<TsdbAggregateSensors, TSDB_COLUMN_COUNT> aggregates_{};
#endif
  StringRef file_;
  StringRef mount_point_;
  StringRef partition_label_;
#ifdef USE_TIME
  time::RealTimeClock *time_{nullptr};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *log_sensor_{nullptr};
#endif
  void *db_{nullptr};

  uint32_t partition_size_{512 * 1024};
  uint32_t max_file_size_{384 * 1024};
  uint32_t index_stride_{tsdbmath::INDEX_DEFAULT_STRIDE};
  uint32_t buffer_pool_size_{4096};
  uint32_t page_size_{2048};
  uint32_t sync_interval_{60000};
  uint32_t min_free_bytes_{32768};
#ifdef USE_TSDB_AGGREGATES
  uint32_t aggregate_window_{3600};
  uint32_t aggregate_interval_{300000};
  uint32_t last_aggregate_{0};
#endif
#ifdef USE_TSDB_DUMP
  uint32_t dump_rows_{60};
  uint32_t dump_requested_{0};
#endif
  uint32_t last_reopen_{0};
  uint32_t last_sync_{0};
  uint32_t last_timestamp_{0};
  // Added to an uptime timestamp so the series continues after the newest stored record.
  uint32_t uptime_offset_{0};
  uint32_t last_stats_{0};
  uint32_t records_{0};
  // Newest durable header slot sequence seen, so a failed slot write is noticed.
  uint32_t newest_sidecar_seq_{0};
  StaticTask sync_task_;
  // Written by the sync task, read and reset by the main loop.
  std::atomic<uint8_t> sync_state_{static_cast<uint8_t>(SyncState::SYNC_STATE_IDLE)};
  uint32_t writes_{0};
  uint32_t write_errors_{0};
  uint32_t reopen_errors_{0};
  uint32_t dropped_{0};
  uint32_t dropped_missing_{0};
  uint32_t dropped_backwards_{0};
  uint32_t uptime_timestamps_{0};
  uint32_t saturated_{0};
  uint32_t held_{0};
  uint32_t sentinel_{0};
  esp_err_t mount_error_{ESP_OK};
  esp_err_t format_error_{ESP_OK};

  tsdbmath::MemoryMode memory_mode_{tsdbmath::MEMORY_AUTO};
  tsdbmath::MissingPolicy missing_policy_{tsdbmath::MISSING_SKIP};
  bool format_on_first_boot_{true};
  bool recreate_on_schema_change_{false};
  bool require_time_{true};
  bool paged_allocation_{false};
  bool mounted_{false};
  bool db_lost_{false};
  bool dirty_{false};
  bool flush_requested_{false};
  bool clear_requested_{false};
  bool format_attempted_{false};
  bool warned_time_{false};
  // Report the next completed sync as the answer to a flush request.
  bool report_flush_{false};
  // Raised by a failed sync and cleared by the next one, never by a successful write.
  bool sync_failed_{false};
};

}  // namespace esphome::tsdb

#endif  // USE_ESP_IDF
