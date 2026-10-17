#pragma once
// Core math of the tsdb logger: int16 encoding and esp_tsdb file geometry.
// Geometry from esp_tsdb 2.4.3; tsdb.cpp static_asserts it against the engine.

#include <cmath>
#include <cstdint>
#include <limits>

namespace esphome::tsdb::tsdbmath {

static constexpr int16_t RAW_MAX = 32767;
static constexpr int16_t RAW_UNKNOWN = -32768;
static constexpr int16_t RAW_MIN = -32767;

enum MissingPolicy : uint8_t {
  MISSING_SKIP = 0,
  MISSING_HOLD = 1,
  MISSING_SENTINEL = 2,
};

// Buffer pool location (mirrors tsdb_alloc_strategy_t).
enum MemoryMode : uint8_t {
  MEMORY_INTERNAL = 0,
  MEMORY_PSRAM = 1,
  MEMORY_AUTO = 2,
};

static constexpr uint32_t BLOCK_SIZE = 1024;
static constexpr uint32_t BLOCK_HEADER_SIZE = 8;
static constexpr uint32_t HEADER_BYTES = 584;
static constexpr uint32_t TIMESTAMP_BYTES = 4;
static constexpr uint32_t COLUMN_BYTES = 2;
static constexpr uint32_t INDEX_ENTRY_BYTES = 8;
static constexpr uint32_t INDEX_DEFAULT_STRIDE = 380;  // mirrors TSDB_DEFAULT_INDEX_STRIDE in __init__.py
static constexpr uint32_t MAX_RECORDS_RESERVE = 2048;

// Header probe for schema changes (offsets mirror tsdb_header_t).
static constexpr uint32_t FILE_MAGIC = 0x45545344u;
static constexpr uint32_t HEADER_MAGIC_OFFSET = 0;
static constexpr uint32_t HEADER_COLUMNS_OFFSET = 8;
static constexpr uint32_t MAX_COLUMNS = 64;

// Stored column count, or 0 when not a readable header (never a reason to delete).
constexpr uint32_t stored_columns(const uint8_t *buffer, size_t length) {
  if (buffer == nullptr || length < HEADER_COLUMNS_OFFSET + 1)
    return 0;
  const uint32_t magic = static_cast<uint32_t>(buffer[0]) | (static_cast<uint32_t>(buffer[1]) << 8) |
                         (static_cast<uint32_t>(buffer[2]) << 16) | (static_cast<uint32_t>(buffer[3]) << 24);
  if (magic != FILE_MAGIC)
    return 0;
  const uint32_t columns = buffer[HEADER_COLUMNS_OFFSET];
  return columns >= 1 && columns <= MAX_COLUMNS ? columns : 0;
}

// Timestamps below this are a boot counter, not a clock.
static constexpr uint32_t MIN_VALID_TIMESTAMP = 1000000000;

inline bool is_missing(float value) { return !std::isfinite(value); }
constexpr bool is_valid_timestamp(uint32_t timestamp) { return timestamp >= MIN_VALID_TIMESTAMP; }
constexpr uint32_t record_bytes(uint32_t columns) { return TIMESTAMP_BYTES + COLUMN_BYTES * columns; }

constexpr uint32_t records_per_block(uint32_t columns) {
  const uint32_t record = record_bytes(columns);
  if (record == 0)
    return 0;
  return (BLOCK_SIZE - BLOCK_HEADER_SIZE) / record;
}

// Upstream TSDB_CALC_MAX_RECORDS(bytes, columns) arithmetic.
constexpr uint32_t max_records_for_bytes(uint64_t bytes, uint32_t columns) {
  const uint32_t record = record_bytes(columns);
  if (record == 0 || bytes <= MAX_RECORDS_RESERVE)
    return 0;
  return static_cast<uint32_t>((bytes - MAX_RECORDS_RESERVE) / record);
}

constexpr uint64_t file_bytes_for(uint32_t records, uint32_t columns, uint32_t index_stride = INDEX_DEFAULT_STRIDE) {
  if (records == 0 || columns == 0 || index_stride == 0)
    return 0;
  const uint32_t per_block = records_per_block(columns);
  if (per_block == 0)
    return 0;
  const uint64_t blocks = (records + per_block - 1) / per_block;
  const uint64_t index_entries = (records + index_stride - 1) / index_stride;
  return HEADER_BYTES + blocks * BLOCK_SIZE + index_entries * INDEX_ENTRY_BYTES;
}

// Shrinks the estimate until file_bytes_for() really fits the budget.
constexpr uint32_t capacity_for_bytes(uint64_t bytes, uint32_t columns, uint32_t index_stride = INDEX_DEFAULT_STRIDE) {
  uint32_t records = max_records_for_bytes(bytes, columns);
  while (records > 0 && file_bytes_for(records, columns, index_stride) > bytes) {
    const uint64_t over = file_bytes_for(records, columns, index_stride) - bytes;
    const uint32_t step = static_cast<uint32_t>(over / record_bytes(columns)) + 1;
    records = step >= records ? 0 : records - step;
  }
  return records;
}

// raw = round(value * scale + offset), clamped to int16.
// *saturated reports that the scaled value fell outside int16 and was clamped,
// so a scale_factor that is too small for the sensor's range does not pass silently.
inline int16_t encode(double value, double scale, double offset = 0.0, bool *saturated = nullptr) {
  if (saturated != nullptr)
    *saturated = false;
  if (!std::isfinite(value) || scale == 0.0)
    return RAW_UNKNOWN;
  const double raw = std::round(value * scale + offset);
  if (raw < static_cast<double>(RAW_MIN)) {
    if (saturated != nullptr)
      *saturated = true;
    return RAW_MIN;
  }
  if (raw > static_cast<double>(RAW_MAX)) {
    if (saturated != nullptr)
      *saturated = true;
    return RAW_MAX;
  }
  return static_cast<int16_t>(raw);
}

// value = (raw - offset) / scale, NaN for RAW_UNKNOWN.
inline double decode(int16_t raw, double scale, double offset = 0.0) {
  if (raw == RAW_UNKNOWN || scale == 0.0)
    return std::numeric_limits<double>::quiet_NaN();
  return (static_cast<double>(raw) - offset) / scale;
}

// Aggregation decode (AVG/MIN/MAX stay in int16 domain).
inline double decode_aggregate(int32_t raw, double scale, double offset = 0.0) {
  if (raw == RAW_UNKNOWN || scale == 0.0)
    return std::numeric_limits<double>::quiet_NaN();
  return (static_cast<double>(raw) - offset) / scale;
}

// Start of a "newest rows" CSV window: rows intervals back from newest with margin.
constexpr uint32_t dump_window_start(uint32_t newest, uint32_t oldest, uint32_t rows, uint32_t interval_seconds,
                                     uint32_t margin = 2) {
  const uint64_t span = static_cast<uint64_t>(rows) * interval_seconds * margin;
  const uint64_t start = span < newest ? static_cast<uint64_t>(newest) - span : 1;
  const uint32_t floor = oldest != 0 ? oldest : 1;
  return start > floor ? static_cast<uint32_t>(start) : floor;
}

// Rows to skip in an over-provisioned window to keep the newest rows.
constexpr uint32_t dump_skip(uint32_t window_records, uint32_t rows) {
  return window_records > rows ? window_records - rows : 0;
}

// True when a short window must widen to the whole history (gap case).
constexpr bool dump_needs_widening(uint32_t window_records, uint32_t rows, uint32_t start, uint32_t oldest) {
  return window_records < rows && start > (oldest != 0 ? oldest : 1);
}

}  // namespace esphome::tsdb::tsdbmath
