#pragma once
// Core math of the tsdb logger: int16 encoding and esp_tsdb 2.4.3 file geometry, checked in tsdb.cpp.

#include <algorithm>
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
// sizeof(tsdb_block_t): the engine's in-memory block, checked in tsdb.cpp.
static constexpr uint32_t BLOCK_IN_MEMORY_BYTES = 1376;
static constexpr uint32_t HEADER_BYTES = 584;
static constexpr uint32_t TIMESTAMP_BYTES = 4;
static constexpr uint32_t COLUMN_BYTES = 2;
static constexpr uint32_t INDEX_ENTRY_BYTES = 8;
static constexpr uint32_t INDEX_OFFSET = 1024;
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

// esp_tsdb's durable header slots (tsdb_internal.h): TSH1 is
// {magic, seq, header_crc, header} and TSH2 {magic, seq, crc, legacy_seq, header
// slices}; both carry a CRC-32 over the bytes that follow it. A torn write is
// therefore only durable when the whole slot and its CRC agree.
static constexpr uint32_t SIDECAR1_MAGIC = 0x54534831u;        // "TSH1"
static constexpr uint32_t SIDECAR2_MAGIC = 0x54534832u;        // "TSH2"
static constexpr uint32_t SIDECAR1_BYTES = 12 + HEADER_BYTES;  // 596
static constexpr uint32_t SIDECAR2_BYTES = 88;                 // checked in tsdb.cpp
static constexpr uint32_t SIDECAR_CRC_OFFSET = 8;
static constexpr uint32_t SIDECAR_BODY_OFFSET = 12;

// CRC-32 (IEEE, reflected), the one esp_tsdb stores in its header slots.
inline uint32_t crc32_ieee(const uint8_t *data, size_t length) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t index = 0; index < length; index++) {
    crc ^= data[index];
    for (uint8_t bit = 0; bit < 8; bit++)
      crc = (crc >> 1) ^ (0xEDB88320u & static_cast<uint32_t>(-static_cast<int32_t>(crc & 1)));
  }
  return ~crc;
}

inline uint32_t read_u32_le(const uint8_t *data) {
  return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
}

// Sequence number of a complete, CRC-valid slot, or 0 when the bytes are not one.
inline uint32_t sidecar_slot_seq(const uint8_t *buffer, size_t length) {
  if (buffer == nullptr || length < SIDECAR_BODY_OFFSET)
    return 0;
  const uint32_t magic = read_u32_le(buffer);
  const size_t expected = magic == SIDECAR1_MAGIC ? SIDECAR1_BYTES : magic == SIDECAR2_MAGIC ? SIDECAR2_BYTES : 0;
  const uint32_t seq = read_u32_le(buffer + 4);
  if (expected == 0 || length != expected || seq == 0)
    return 0;
  if (crc32_ieee(buffer + SIDECAR_BODY_OFFSET, length - SIDECAR_BODY_OFFSET) !=
      read_u32_le(buffer + SIDECAR_CRC_OFFSET))
    return 0;
  return seq;
}

// Schema limits, mirrored from TSDB_MAX_BASE_PARAMS and TSDB_MAX_NAME_LENGTH in __init__.py.
static constexpr uint32_t MAX_BASE_PARAMS = 16;
static constexpr uint32_t MAX_NAME_LENGTH = 31;

inline bool is_missing(float value) { return !std::isfinite(value); }
constexpr bool is_valid_timestamp(uint32_t timestamp) { return timestamp >= MIN_VALID_TIMESTAMP; }
constexpr uint32_t record_bytes(uint32_t columns) { return TIMESTAMP_BYTES + COLUMN_BYTES * columns; }

constexpr uint32_t records_per_block(uint32_t columns) {
  const uint32_t record = record_bytes(columns);
  if (record == 0)
    return 0;
  return (BLOCK_SIZE - BLOCK_HEADER_SIZE) / record;
}

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
  const uint64_t blocks = (static_cast<uint64_t>(records) + per_block - 1) / per_block;
  const uint64_t index_entries = static_cast<uint64_t>(records) / index_stride + 1;
  return INDEX_OFFSET + blocks * BLOCK_SIZE + index_entries * INDEX_ENTRY_BYTES;
}

// Largest capacity that fits the budget: binary search, since a greedy shrink can skip past zero.
constexpr uint32_t capacity_for_bytes(uint64_t bytes, uint32_t columns, uint32_t index_stride = INDEX_DEFAULT_STRIDE) {
  if (columns == 0 || index_stride == 0 || records_per_block(columns) == 0)
    return 0;
  uint32_t low = 0;
  uint32_t high = max_records_for_bytes(bytes, columns);
  while (low < high) {
    const uint32_t mid = low + (high - low) / 2 + 1;
    if (file_bytes_for(mid, columns, index_stride) <= bytes) {
      low = mid;
    } else {
      high = mid - 1;
    }
  }
  return low;
}

// raw = round(value * scale + offset), clamped to int16; *saturated reports a clamp.
inline int16_t encode(double value, double scale, double offset = 0.0, bool *saturated = nullptr) {
  if (saturated != nullptr)
    *saturated = false;
  // A non-finite scale or offset (YAML .nan/.inf) would reach an undefined int16 cast.
  if (!std::isfinite(value) || !std::isfinite(scale) || !std::isfinite(offset) || scale == 0.0)
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

// Average, minimum and maximum over decoded samples. Decoding before comparing keeps
// the order right on a negative scale, and the unknown sentinel is not a sample.
class Aggregate {
 public:
  void add_raw(int16_t raw, double scale, double offset = 0.0) {
    if (raw == RAW_UNKNOWN)
      return;
    const double value = decode(raw, scale, offset);
    if (this->count_ == 0) {
      this->min_ = value;
      this->max_ = value;
    } else {
      this->min_ = std::min(this->min_, value);
      this->max_ = std::max(this->max_, value);
    }
    this->sum_ += value;
    this->count_++;
  }

  uint32_t count() const { return this->count_; }
  double average() const {
    return this->count_ == 0 ? std::numeric_limits<double>::quiet_NaN() : this->sum_ / this->count_;
  }
  double min() const { return this->count_ == 0 ? std::numeric_limits<double>::quiet_NaN() : this->min_; }
  double max() const { return this->count_ == 0 ? std::numeric_limits<double>::quiet_NaN() : this->max_; }

 protected:
  double sum_{0.0};
  double min_{0.0};
  double max_{0.0};
  uint32_t count_{0};
};

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
