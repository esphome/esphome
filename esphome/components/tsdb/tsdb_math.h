#pragma once
//
// Pure math of the tsdb data logger: the int16_t encoding of a sensor reading,
// the file geometry of the esp_tsdb engine and the retention it buys.
//
// Geometry mirrored from esp_tsdb 2.4.3 (MIT, https://github.com/zakery292/esp_tsdb),
// taken from include/esp_tsdb.h and src/tsdb_internal.h:
//
//   sizeof(tsdb_header_t)                         = 584 bytes (packed struct; the
//                                                   "512" in the upstream README is stale)
//   TSDB_BLOCK_SIZE                               = 1024 bytes, 8 byte block header
//   record                                        = 4 byte timestamp + 2 bytes per column
//   index                                         = 8 bytes per index_stride records
//   TSDB_CALC_MAX_RECORDS(bytes, columns)         = (bytes - 2048) / (4 + 2 * columns)
//
// `tsdb.cpp` static_asserts these helpers against the real macros of the engine,
// so a drift in esp_tsdb breaks the build instead of the documentation.
//
// This header is intentionally free of ESPHome / ESP-IDF dependencies so the very
// same code runs in the host test (tests/components/tsdb/tsdb_math_test.cpp).

#include <cmath>
#include <cstdint>
#include <limits>

namespace esphome::tsdb::tsdbmath {

/// Highest value the engine stores per column (int16_t).
static constexpr int16_t RAW_MAX = 32767;
/// Lowest int16_t, reserved as the "no value" marker written by `on_missing: sentinel`.
static constexpr int16_t RAW_UNKNOWN = -32768;
/// A reading is clamped here first, so a saturated value is never mistaken for
/// RAW_UNKNOWN.
static constexpr int16_t RAW_MIN = -32767;

/// How a column without a fresh value is handled (mirrors the Python mapping).
enum MissingPolicy : uint8_t {
  MISSING_SKIP = 0,      ///< drop the whole record
  MISSING_HOLD = 1,      ///< repeat the last value written for that column
  MISSING_SENTINEL = 2,  ///< write RAW_UNKNOWN for that column
};

/// Buffer pool location (mirrors tsdb_alloc_strategy_t).
enum MemoryMode : uint8_t {
  MEMORY_INTERNAL = 0,  ///< TSDB_ALLOC_INTERNAL_RAM
  MEMORY_PSRAM = 1,     ///< TSDB_ALLOC_PSRAM
  MEMORY_AUTO = 2,      ///< TSDB_ALLOC_AUTO
};

/// esp_tsdb file geometry (esp_tsdb 2.4.3).
static constexpr uint32_t BLOCK_SIZE = 1024;
static constexpr uint32_t BLOCK_HEADER_SIZE = 8;
static constexpr uint32_t HEADER_BYTES = 584;  ///< sizeof(tsdb_header_t)
static constexpr uint32_t TIMESTAMP_BYTES = 4;
static constexpr uint32_t COLUMN_BYTES = 2;
static constexpr uint32_t INDEX_ENTRY_BYTES = 8;
static constexpr uint32_t INDEX_DEFAULT_STRIDE = 380;
/// What TSDB_CALC_MAX_RECORDS() keeps for the header and the sparse index.
static constexpr uint32_t MAX_RECORDS_RESERVE = 2048;

/// What an existing esp_tsdb file says about the schema it was written with. The
/// engine keeps that in the first bytes of the file, so it can be read without
/// the engine - and without laying out `tsdb_header_t` on the host. `tsdb.cpp`
/// static_asserts the offsets against `offsetof(tsdb_header_t, ...)`, so an
/// engine release that reorders the header breaks the build.
static constexpr uint32_t FILE_MAGIC = 0x45545344u;   ///< TSDB_MAGIC, "ETSD"
static constexpr uint32_t HEADER_MAGIC_OFFSET = 0;    ///< offsetof(tsdb_header_t, magic)
static constexpr uint32_t HEADER_COLUMNS_OFFSET = 8;  ///< offsetof(tsdb_header_t, num_params)
/// TSDB_MAX_PARAMS, the engine's own upper bound for a stored column count.
static constexpr uint32_t MAX_COLUMNS = 64;

/// The column count an esp_tsdb header was written with, or 0 when `buffer` is
/// not a header this component may act on (too short, wrong magic, a count
/// outside `1..MAX_COLUMNS`).
///
/// `0` means *not proven to be a schema mismatch*, and a caller must not delete
/// anything for it: esp_tsdb allocates its buffer pool **before** it touches the
/// file, so an open failure can be an out-of-memory, filesystem or I/O problem on
/// a perfectly healthy database - and deleting that history would be worse than a
/// boot without a database. The bytes are read explicitly little-endian, so the
/// probe depends on neither struct layout, padding, aliasing nor the host's
/// endianness.
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

/// Timestamps below this are a boot counter, not a clock: 2001-09-09T01:46:40Z.
static constexpr uint32_t MIN_VALID_TIMESTAMP = 1000000000;

/// A reading that cannot be logged (NaN, infinity).
inline bool is_missing(float value) { return !std::isfinite(value); }

/// Whether a timestamp comes from a real clock (see MIN_VALID_TIMESTAMP).
constexpr bool is_valid_timestamp(uint32_t timestamp) { return timestamp >= MIN_VALID_TIMESTAMP; }

/// Bytes one record occupies in a block, for `columns` columns.
constexpr uint32_t record_bytes(uint32_t columns) { return TIMESTAMP_BYTES + COLUMN_BYTES * columns; }

/// Records that fit in one 1 KB block, for `columns` columns.
constexpr uint32_t records_per_block(uint32_t columns) {
  const uint32_t record = record_bytes(columns);
  if (record == 0)
    return 0;
  return (BLOCK_SIZE - BLOCK_HEADER_SIZE) / record;
}

/// Records that fit into `bytes` of database file - the arithmetic of the
/// upstream TSDB_CALC_MAX_RECORDS(bytes, columns) macro.
constexpr uint32_t max_records_for_bytes(uint64_t bytes, uint32_t columns) {
  const uint32_t record = record_bytes(columns);
  if (record == 0 || bytes <= MAX_RECORDS_RESERVE)
    return 0;
  return static_cast<uint32_t>((bytes - MAX_RECORDS_RESERVE) / record);
}

/// Size the data file needs for `records` records: the 584-byte header plus
/// whole 1 KB blocks plus the sparse index. It is an upper bound (the last
/// block counts as full) and the source of the numbers quoted in
/// docs/data_logging.md.
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

/// Largest number of records whose file still fits into `bytes`.
///
/// TSDB_CALC_MAX_RECORDS() ignores that a block only holds
/// `(1024 - 8) / record_size` records (63 of the 1008 bytes a 6-column block
/// uses), so its capacity overflows the budget by ~1.5 % - the solver here
/// shrinks the estimate until `file_bytes_for()` really fits.
constexpr uint32_t capacity_for_bytes(uint64_t bytes, uint32_t columns, uint32_t index_stride = INDEX_DEFAULT_STRIDE) {
  uint32_t records = max_records_for_bytes(bytes, columns);
  while (records > 0 && file_bytes_for(records, columns, index_stride) > bytes) {
    const uint64_t over = file_bytes_for(records, columns, index_stride) - bytes;
    const uint32_t step = static_cast<uint32_t>(over / record_bytes(columns)) + 1;
    records = step >= records ? 0 : records - step;
  }
  return records;
}

/// Seconds of history `records` records cover at `interval_seconds` each.
constexpr uint64_t history_seconds(uint32_t records, uint32_t interval_seconds) {
  return static_cast<uint64_t>(records) * interval_seconds;
}

/// Timestamp a "newest `rows`" CSV dump starts its query from.
///
/// The engine's query can jump to its start timestamp in O(log n) block reads
/// (`tsdb_seek_start()`), but only while that start is *later* than the oldest
/// retained record: a query that begins at `oldest_timestamp` walks the whole
/// history block by block. Selecting the window by row count (the older
/// behaviour) therefore read - and discarded - every older record on each press,
/// which at full capacity meant ~24 000 records in a single `update()`.
///
/// This estimates the window from the write cadence instead: `rows` write
/// intervals back from `newest`, over-provisioned by `margin` so timestamp
/// jitter and a gap in the history cannot leave the window short of `rows`
/// records. The result is clamped to `oldest` (a database younger than the
/// window is dumped from its first row) and to at least 1, so it never
/// underflows and never leaves the engine's range.
constexpr uint32_t dump_window_start(uint32_t newest, uint32_t oldest, uint32_t rows, uint32_t interval_seconds,
                                     uint32_t margin = 2) {
  const uint64_t span = static_cast<uint64_t>(rows) * interval_seconds * margin;
  const uint64_t start = span < newest ? static_cast<uint64_t>(newest) - span : 1;
  const uint32_t floor = oldest != 0 ? oldest : 1;
  return start > floor ? static_cast<uint32_t>(start) : floor;
}

/// Rows the CSV dump walks past inside its (time-selected) window so that the
/// **newest** `rows` of it are the ones printed.
///
/// `dump_window_start()` over-provisions the window by a margin, so the query
/// can land a few records before the newest `rows`; those are skipped here.
/// `window_records` is the number of records actually *in* the window (from
/// `tsdb_query_count_h()`), never the capacity. A window holding fewer records
/// than asked for skips nothing and the dump prints all of them - but only after
/// `dump_needs_widening()` had its say: a short window is normally widened to the
/// whole history first, and the remaining short case is a database that really
/// holds fewer rows (its rows all fit).
constexpr uint32_t dump_skip(uint32_t window_records, uint32_t rows) {
  return window_records > rows ? window_records - rows : 0;
}

/// Whether a short time-selected dump window has to be widened to the whole
/// history to keep the documented "the newest `dump_rows` rows **that are
/// stored**".
///
/// `dump_window_start()` estimates the window from the write cadence, so a gap in
/// the history that is longer than the margin leaves the window with fewer
/// records than asked for *although older records exist* (`on_missing: skip`
/// writes nothing while a sensor is out - that is what the gaps in the history
/// are). Widening once to `oldest` is exact, it can hold every retained record;
/// it does cost the block-by-block walk the time-selected window was introduced
/// to avoid, but only in that gap case. And it costs no second count pass: the
/// widened range is the whole ring, so its record count is the `total_records`
/// the statistics of the caller already reported. A file that holds fewer than
/// `rows` records cannot be satisfied by any window, so this reports `false`
/// there.
constexpr bool dump_needs_widening(uint32_t window_records, uint32_t rows, uint32_t start, uint32_t oldest) {
  return window_records < rows && start > (oldest != 0 ? oldest : 1);
}

/// Encode an engineering value as the raw int16_t the engine stores:
/// `raw = round(value * scale + offset)`, clamped to the int16_t range.
/// A value that cannot be encoded at all (NaN, infinite, `scale == 0`) is
/// reported as RAW_UNKNOWN and must be filtered by the caller beforehand when
/// `raw == RAW_UNKNOWN` is reserved for "no value".
inline int16_t encode(double value, double scale, double offset = 0.0) {
  if (!std::isfinite(value) || scale == 0.0)
    return RAW_UNKNOWN;
  const double raw = std::round(value * scale + offset);
  if (raw < static_cast<double>(RAW_MIN))
    return RAW_MIN;
  if (raw > static_cast<double>(RAW_MAX))
    return RAW_MAX;
  return static_cast<int16_t>(raw);
}

/// Decode an aggregation result. AVG/MIN/MAX keep the int16_t domain of the
/// column; SUM and COUNT do not, so only the three are exposed as sensors.
inline double decode_aggregate(int32_t raw, double scale, double offset = 0.0) {
  if (raw == RAW_UNKNOWN || scale == 0.0)
    return std::numeric_limits<double>::quiet_NaN();
  return (static_cast<double>(raw) - offset) / scale;
}

/// Decode a raw column value back into engineering units:
/// `value = (raw - offset) / scale`. Returns NaN for the RAW_UNKNOWN marker.
inline double decode(int16_t raw, double scale, double offset = 0.0) {
  if (raw == RAW_UNKNOWN || scale == 0.0)
    return std::numeric_limits<double>::quiet_NaN();
  return (static_cast<double>(raw) - offset) / scale;
}

}  // namespace esphome::tsdb::tsdbmath
