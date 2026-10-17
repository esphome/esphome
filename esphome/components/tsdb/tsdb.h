#pragma once

// tsdb hub header; ESP-IDF only (esp_tsdb + LittleFS engine).
#ifdef USE_ESP_IDF

#include <cstdint>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/string_ref.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/time/real_time_clock.h"

#include "esp_err.h"

#include "tsdb_math.h"

namespace esphome::tsdb {

struct TsdbColumn {
  StringRef name;
  sensor::Sensor *source{nullptr};
  double scale{1.0};
  double offset{0.0};
  int16_t held{tsdbmath::RAW_UNKNOWN};
};

class TsdbComponent : public PollingComponent {
#ifdef USE_SENSOR
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
  void set_max_file_size(uint32_t bytes) { this->max_file_size_ = bytes; }
  void set_index_stride(uint32_t stride) { this->index_stride_ = stride; }
  void set_buffer_pool_size(uint32_t bytes) { this->buffer_pool_size_ = bytes; }
  void set_sync_interval(uint32_t ms) { this->sync_interval_ = ms; }
  void set_min_free_bytes(uint32_t bytes) { this->min_free_bytes_ = bytes; }
  void set_require_time(bool require) { this->require_time_ = require; }
  void set_missing_policy(tsdbmath::MissingPolicy policy) { this->missing_policy_ = policy; }
  void set_time_source(time::RealTimeClock *clock) { this->time_ = clock; }

  void add_column(const char *name, double scale, double offset);
  void set_column_source(size_t index, sensor::Sensor *source);

  bool is_ready() const { return this->db_ != nullptr; }

 protected:
  bool mount_filesystem_();
  void unmount_filesystem_();
  bool open_database_();
  void log_init_failure_();
  void fail_setup_(const LogString *reason);
  void close_database_();
  bool write_record_();
  void publish_errors_();
  void flush_(bool force);
  static uint64_t free_space_probe();

  StringRef file_{"history.tsdb"};
  StringRef mount_point_{"/littlefs"};
  StringRef partition_label_{"littlefs"};
  uint32_t partition_size_{512 * 1024};
  bool format_on_first_boot_{true};
  uint32_t max_file_size_{384 * 1024};
  uint32_t index_stride_{tsdbmath::INDEX_DEFAULT_STRIDE};
  uint32_t buffer_pool_size_{4096};
  uint32_t sync_interval_{60000};
  uint32_t min_free_bytes_{32768};
  bool require_time_{true};
  tsdbmath::MissingPolicy missing_policy_{tsdbmath::MISSING_SKIP};
  std::vector<TsdbColumn> columns_{};
  std::vector<int16_t> values_{};
  time::RealTimeClock *time_{nullptr};

  void *db_{nullptr};
  bool mounted_{false};
  uint32_t writes_{0};
  uint32_t write_errors_{0};
  uint32_t dropped_{0};
  uint32_t saturated_{0};
  uint32_t last_sync_{0};
  bool dirty_{false};
  bool warned_time_{false};
  esp_err_t mount_error_{ESP_OK};
  bool format_attempted_{false};
  esp_err_t format_error_{ESP_OK};
};

}  // namespace esphome::tsdb

#endif  // USE_ESP_IDF
