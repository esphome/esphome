/**
 * Benchmark: ProtoWriteBuffer encoding performance
 *
 * Compares the old push_back()-based encoding against the new pre-sized
 * pointer-write approach introduced in PR #14018.
 *
 * Build (from repo root):
 *   g++ -std=gnu++20 -O2 \
 *       tests/benchmarks/proto_encode_benchmark.cpp \
 *       -o tests/benchmarks/proto_encode_benchmark
 *
 * For ESP-like size-optimized builds (-Os):
 *   g++ -std=gnu++20 -Os \
 *       tests/benchmarks/proto_encode_benchmark.cpp \
 *       -o tests/benchmarks/proto_encode_benchmark
 *
 * Run:
 *   ./tests/benchmarks/proto_encode_benchmark
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <string>
#include <vector>

// ============================================================================
// Minimal stubs to avoid pulling in the full ESPHome framework
// ============================================================================

namespace esphome {

class StringRef {
 public:
  constexpr StringRef() : base_(""), len_(0) {}
  explicit StringRef(const char *s) : base_(s), len_(strlen(s)) {}
  constexpr StringRef(const char *s, size_t n) : base_(s), len_(n) {}
  explicit StringRef(const std::string &s) : base_(s.c_str()), len_(s.size()) {}

  const char *c_str() const { return base_; }
  size_t size() const { return len_; }
  bool empty() const { return len_ == 0; }

 private:
  const char *base_;
  size_t len_;
};

}  // namespace esphome

// ============================================================================
// Old-style ProtoWriteBuffer (push_back based) - from dev branch
// ============================================================================

class OldProtoWriteBuffer {
 public:
  explicit OldProtoWriteBuffer(std::vector<uint8_t> *buffer) : buffer_(buffer) {}

  void encode_varint_raw(uint32_t value) {
    while (value > 0x7F) {
      this->buffer_->push_back(static_cast<uint8_t>(value | 0x80));
      value >>= 7;
    }
    this->buffer_->push_back(static_cast<uint8_t>(value));
  }

  void encode_varint_raw_64(uint64_t value) {
    while (value > 0x7F) {
      this->buffer_->push_back(static_cast<uint8_t>(value | 0x80));
      value >>= 7;
    }
    this->buffer_->push_back(static_cast<uint8_t>(value));
  }

  void encode_field_raw(uint32_t field_id, uint32_t type) { this->encode_varint_raw((field_id << 3) | type); }

  void encode_string(uint32_t field_id, const char *string, size_t len, bool force = false) {
    if (len == 0 && !force)
      return;
    this->encode_field_raw(field_id, 2);
    this->encode_varint_raw(len);
    size_t old_size = this->buffer_->size();
    this->buffer_->resize(old_size + len);
    std::memcpy(this->buffer_->data() + old_size, string, len);
  }

  void encode_string(uint32_t field_id, const esphome::StringRef &ref, bool force = false) {
    this->encode_string(field_id, ref.c_str(), ref.size(), force);
  }

  void encode_uint32(uint32_t field_id, uint32_t value, bool force = false) {
    if (value == 0 && !force)
      return;
    this->encode_field_raw(field_id, 0);
    this->encode_varint_raw(value);
  }

  void encode_bool(uint32_t field_id, bool value, bool force = false) {
    if (!value && !force)
      return;
    this->encode_field_raw(field_id, 0);
    this->buffer_->push_back(value ? 0x01 : 0x00);
  }

  void encode_fixed32(uint32_t field_id, uint32_t value, bool force = false) {
    if (value == 0 && !force)
      return;
    this->encode_field_raw(field_id, 5);
    this->buffer_->push_back((value >> 0) & 0xFF);
    this->buffer_->push_back((value >> 8) & 0xFF);
    this->buffer_->push_back((value >> 16) & 0xFF);
    this->buffer_->push_back((value >> 24) & 0xFF);
  }

  void encode_float(uint32_t field_id, float value, bool force = false) {
    if (value == 0.0f && !force)
      return;
    union {
      float value;
      uint32_t raw;
    } val{};
    val.value = value;
    this->encode_fixed32(field_id, val.raw);
  }

  void encode_bytes(uint32_t field_id, const uint8_t *data, size_t len, bool force = false) {
    this->encode_string(field_id, reinterpret_cast<const char *>(data), len, force);
  }

  std::vector<uint8_t> *get_buffer() const { return buffer_; }

 protected:
  std::vector<uint8_t> *buffer_;
};

// ============================================================================
// New-style ProtoWriteBuffer (pointer-write based) - from this PR
// ============================================================================

class NewProtoWriteBuffer {
 public:
  NewProtoWriteBuffer(std::vector<uint8_t> *buffer, size_t write_pos)
      : buffer_(buffer), pos_(buffer->data() + write_pos) {}

  void encode_varint_raw(uint32_t value) {
    while (value > 0x7F) {
      *this->pos_++ = static_cast<uint8_t>(value | 0x80);
      value >>= 7;
    }
    *this->pos_++ = static_cast<uint8_t>(value);
  }

  void encode_varint_raw_64(uint64_t value) {
    while (value > 0x7F) {
      *this->pos_++ = static_cast<uint8_t>(value | 0x80);
      value >>= 7;
    }
    *this->pos_++ = static_cast<uint8_t>(value);
  }

  void encode_field_raw(uint32_t field_id, uint32_t type) { this->encode_varint_raw((field_id << 3) | type); }

  void encode_string(uint32_t field_id, const char *string, size_t len, bool force = false) {
    if (len == 0 && !force)
      return;
    this->encode_field_raw(field_id, 2);
    this->encode_varint_raw(len);
    std::memcpy(this->pos_, string, len);
    this->pos_ += len;
  }

  void encode_string(uint32_t field_id, const esphome::StringRef &ref, bool force = false) {
    this->encode_string(field_id, ref.c_str(), ref.size(), force);
  }

  void encode_uint32(uint32_t field_id, uint32_t value, bool force = false) {
    if (value == 0 && !force)
      return;
    this->encode_field_raw(field_id, 0);
    this->encode_varint_raw(value);
  }

  void encode_bool(uint32_t field_id, bool value, bool force = false) {
    if (!value && !force)
      return;
    this->encode_field_raw(field_id, 0);
    *this->pos_++ = value ? 0x01 : 0x00;
  }

  void encode_fixed32(uint32_t field_id, uint32_t value, bool force = false) {
    if (value == 0 && !force)
      return;
    this->encode_field_raw(field_id, 5);
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    std::memcpy(this->pos_, &value, 4);
    this->pos_ += 4;
#else
    *this->pos_++ = (value >> 0) & 0xFF;
    *this->pos_++ = (value >> 8) & 0xFF;
    *this->pos_++ = (value >> 16) & 0xFF;
    *this->pos_++ = (value >> 24) & 0xFF;
#endif
  }

  void encode_float(uint32_t field_id, float value, bool force = false) {
    if (value == 0.0f && !force)
      return;
    union {
      float value;
      uint32_t raw;
    } val{};
    val.value = value;
    this->encode_fixed32(field_id, val.raw);
  }

  void encode_bytes(uint32_t field_id, const uint8_t *data, size_t len, bool force = false) {
    this->encode_string(field_id, reinterpret_cast<const char *>(data), len, force);
  }

  uint8_t *pos() const { return pos_; }
  std::vector<uint8_t> *get_buffer() const { return buffer_; }

 protected:
  std::vector<uint8_t> *buffer_;
  uint8_t *pos_;
};

// ============================================================================
// ProtoSize - calculate exact encoded size (shared by both approaches)
// ============================================================================

class ProtoSize {
 public:
  static constexpr uint32_t varint(uint32_t value) {
    if (value < 128)
      return 1;
    if (value < 16384)
      return 2;
    if (value < 2097152)
      return 3;
    if (value < 268435456)
      return 4;
    return 5;
  }

  static constexpr uint32_t field(uint32_t field_id, uint32_t type) { return varint((field_id << 3) | (type & 0x7)); }

  static constexpr uint32_t calc_uint32(uint32_t field_id_size, uint32_t value) {
    return value ? field_id_size + varint(value) : 0;
  }

  static constexpr uint32_t calc_bool(uint32_t field_id_size, bool value) { return value ? field_id_size + 1 : 0; }

  static constexpr uint32_t calc_float(uint32_t field_id_size, float value) {
    return value != 0.0f ? field_id_size + 4 : 0;
  }

  static constexpr uint32_t calc_fixed32(uint32_t field_id_size, uint32_t value) {
    return value ? field_id_size + 4 : 0;
  }

  static constexpr uint32_t calc_length(uint32_t field_id_size, size_t len) {
    return len ? field_id_size + varint(static_cast<uint32_t>(len)) + static_cast<uint32_t>(len) : 0;
  }
};

// ============================================================================
// Benchmark infrastructure
// ============================================================================

struct BenchResult {
  const char *name;
  double ns_per_op;
  double ops_per_sec;
  size_t iterations;
  size_t bytes_per_op;
};

// Prevent compiler from optimizing away the result
template<typename T> __attribute__((noinline)) void do_not_optimize(T &value) {
  asm volatile("" : "+r,m"(value) : : "memory");
}

__attribute__((noinline)) void clobber_memory() { asm volatile("" : : : "memory"); }

template<typename Func> BenchResult benchmark(const char *name, size_t bytes_per_op, Func func) {
  // Warmup
  for (int i = 0; i < 1000; i++) {
    func();
  }

  // Determine iteration count (target ~100ms)
  size_t iterations = 1000;
  auto start = std::chrono::high_resolution_clock::now();
  for (size_t i = 0; i < iterations; i++) {
    func();
  }
  auto end = std::chrono::high_resolution_clock::now();
  double elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  double ns_per_op = elapsed_ns / iterations;

  // Scale iterations to target ~200ms
  iterations = std::max<size_t>(10000, static_cast<size_t>(200'000'000.0 / ns_per_op));

  // Actual benchmark run
  start = std::chrono::high_resolution_clock::now();
  for (size_t i = 0; i < iterations; i++) {
    func();
    clobber_memory();
  }
  end = std::chrono::high_resolution_clock::now();
  elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  ns_per_op = elapsed_ns / iterations;

  return BenchResult{name, ns_per_op, 1'000'000'000.0 / ns_per_op, iterations, bytes_per_op};
}

void print_results(const std::vector<BenchResult> &results) {
  printf("%-50s %12s %12s %12s %10s\n", "Benchmark", "ns/op", "ops/sec", "iters", "bytes/op");
  printf("%-50s %12s %12s %12s %10s\n", std::string(50, '-').c_str(), "--------", "--------", "--------", "--------");
  for (const auto &r : results) {
    printf("%-50s %12.1f %12.0f %12zu %10zu\n", r.name, r.ns_per_op, r.ops_per_sec, r.iterations, r.bytes_per_op);
  }
}

void print_comparison(const char *label, const BenchResult &old_result, const BenchResult &new_result) {
  double speedup = old_result.ns_per_op / new_result.ns_per_op;
  printf("  %-46s  %.1fx %s\n", label, speedup, speedup > 1.0 ? "faster" : "slower");
}

// ============================================================================
// Benchmark: Varint encoding
// ============================================================================

static void bench_varint_old(std::vector<uint8_t> &buf) {
  buf.clear();
  OldProtoWriteBuffer writer(&buf);
  // Encode a mix of varint sizes (1-5 bytes)
  writer.encode_varint_raw(0x01);        // 1 byte
  writer.encode_varint_raw(0x80);        // 2 bytes
  writer.encode_varint_raw(0x4000);      // 3 bytes
  writer.encode_varint_raw(0x200000);    // 4 bytes
  writer.encode_varint_raw(0x10000000);  // 5 bytes
}

static void bench_varint_new(std::vector<uint8_t> &buf, size_t size) {
  buf.resize(size);
  NewProtoWriteBuffer writer(&buf, 0);
  writer.encode_varint_raw(0x01);
  writer.encode_varint_raw(0x80);
  writer.encode_varint_raw(0x4000);
  writer.encode_varint_raw(0x200000);
  writer.encode_varint_raw(0x10000000);
}

// ============================================================================
// Benchmark: String encoding (simulates entity names, object_ids, etc.)
// ============================================================================

static const char SHORT_STR[] = "sensor_1";                         // 8 bytes
static const char MEDIUM_STR[] = "living_room_temperature_sensor";  // 30 bytes
static const char LONG_STR[] =
    "esphome_very_long_device_name_with_many_characters_for_testing_purposes_abcdef";  // 78 bytes

static void bench_strings_old(std::vector<uint8_t> &buf) {
  buf.clear();
  OldProtoWriteBuffer writer(&buf);
  writer.encode_string(1, SHORT_STR, strlen(SHORT_STR));
  writer.encode_string(2, MEDIUM_STR, strlen(MEDIUM_STR));
  writer.encode_string(3, LONG_STR, strlen(LONG_STR));
}

static size_t calc_strings_size() {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, strlen(SHORT_STR));
  size += ProtoSize::calc_length(1, strlen(MEDIUM_STR));
  size += ProtoSize::calc_length(1, strlen(LONG_STR));
  return size;
}

static void bench_strings_new(std::vector<uint8_t> &buf, size_t size) {
  buf.resize(size);
  NewProtoWriteBuffer writer(&buf, 0);
  writer.encode_string(1, SHORT_STR, strlen(SHORT_STR));
  writer.encode_string(2, MEDIUM_STR, strlen(MEDIUM_STR));
  writer.encode_string(3, LONG_STR, strlen(LONG_STR));
}

// ============================================================================
// Benchmark: Fixed32 encoding (simulates key fields in state responses)
// ============================================================================

static void bench_fixed32_old(std::vector<uint8_t> &buf) {
  buf.clear();
  OldProtoWriteBuffer writer(&buf);
  for (uint32_t i = 1; i <= 10; i++) {
    writer.encode_fixed32(i, 0xDEADBEEF);
  }
}

static size_t calc_fixed32_size() {
  uint32_t size = 0;
  for (uint32_t i = 1; i <= 10; i++) {
    size += ProtoSize::calc_fixed32(1, 0xDEADBEEF);
  }
  return size;
}

static void bench_fixed32_new(std::vector<uint8_t> &buf, size_t size) {
  buf.resize(size);
  NewProtoWriteBuffer writer(&buf, 0);
  for (uint32_t i = 1; i <= 10; i++) {
    writer.encode_fixed32(i, 0xDEADBEEF);
  }
}

// ============================================================================
// Benchmark: Simulate SensorStateResponse encoding
// SensorStateResponse has: fixed32 key, float state, bool missing_state
// This is the most frequent message type during normal operation.
// ============================================================================

static void bench_sensor_state_old(std::vector<uint8_t> &buf) {
  buf.clear();
  OldProtoWriteBuffer writer(&buf);
  writer.encode_fixed32(1, 0x12345678);  // key
  writer.encode_float(2, 23.5f);         // state
  writer.encode_bool(3, false);          // missing_state (default, skipped)
}

static size_t calc_sensor_state_size() {
  uint32_t size = 0;
  size += ProtoSize::calc_fixed32(1, 0x12345678);
  size += ProtoSize::calc_float(1, 23.5f);
  size += ProtoSize::calc_bool(1, false);
  return size;
}

static void bench_sensor_state_new(std::vector<uint8_t> &buf, size_t size) {
  buf.resize(size);
  NewProtoWriteBuffer writer(&buf, 0);
  writer.encode_fixed32(1, 0x12345678);
  writer.encode_float(2, 23.5f);
  writer.encode_bool(3, false);
}

// ============================================================================
// Benchmark: Simulate ListEntitiesSensorResponse encoding
// This is a larger message sent during entity listing.
// Fields: object_id, key, name, unique_id, icon, unit_of_measurement,
//         accuracy_decimals, force_update, device_class, state_class
// ============================================================================

static const char OBJ_ID[] = "living_room_temp";
static const char NAME[] = "Living Room Temperature";
static const char UNIQUE_ID[] = "esp32_01-sensor-living_room_temp";
static const char ICON[] = "mdi:thermometer";
static const char UNIT[] = "\xc2\xb0"
                           "C";  // UTF-8 degree C
static const char DEVICE_CLASS[] = "temperature";

static void bench_list_entities_old(std::vector<uint8_t> &buf) {
  buf.clear();
  OldProtoWriteBuffer writer(&buf);
  writer.encode_string(1, OBJ_ID, strlen(OBJ_ID));              // object_id
  writer.encode_fixed32(2, 0xABCD1234);                         // key
  writer.encode_string(3, NAME, strlen(NAME));                  // name
  writer.encode_string(4, UNIQUE_ID, strlen(UNIQUE_ID));        // unique_id
  writer.encode_string(5, ICON, strlen(ICON));                  // icon
  writer.encode_string(6, UNIT, strlen(UNIT));                  // unit_of_measurement
  writer.encode_uint32(7, 1);                                   // accuracy_decimals
  writer.encode_bool(8, false);                                 // force_update
  writer.encode_string(9, DEVICE_CLASS, strlen(DEVICE_CLASS));  // device_class
  writer.encode_uint32(10, 1);                                  // state_class
}

static size_t calc_list_entities_size() {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, strlen(OBJ_ID));
  size += ProtoSize::calc_fixed32(1, 0xABCD1234);
  size += ProtoSize::calc_length(1, strlen(NAME));
  size += ProtoSize::calc_length(1, strlen(UNIQUE_ID));
  size += ProtoSize::calc_length(1, strlen(ICON));
  size += ProtoSize::calc_length(1, strlen(UNIT));
  size += ProtoSize::calc_uint32(1, 1);
  size += ProtoSize::calc_bool(1, false);
  size += ProtoSize::calc_length(1, strlen(DEVICE_CLASS));
  size += ProtoSize::calc_uint32(1, 1);
  return size;
}

static void bench_list_entities_new(std::vector<uint8_t> &buf, size_t size) {
  buf.resize(size);
  NewProtoWriteBuffer writer(&buf, 0);
  writer.encode_string(1, OBJ_ID, strlen(OBJ_ID));
  writer.encode_fixed32(2, 0xABCD1234);
  writer.encode_string(3, NAME, strlen(NAME));
  writer.encode_string(4, UNIQUE_ID, strlen(UNIQUE_ID));
  writer.encode_string(5, ICON, strlen(ICON));
  writer.encode_string(6, UNIT, strlen(UNIT));
  writer.encode_uint32(7, 1);
  writer.encode_bool(8, false);
  writer.encode_string(9, DEVICE_CLASS, strlen(DEVICE_CLASS));
  writer.encode_uint32(10, 1);
}

// ============================================================================
// Benchmark: Simulate BLE advertisement batch encoding
// BluetoothLERawAdvertisementsResponse with multiple advertisements.
// Each advert has: uint64 address, sint32 rssi, uint32 address_type, bytes data
// This is a high-frequency message that benefits most from optimization.
// ============================================================================

static const uint8_t FAKE_BLE_DATA[31] = {0x02, 0x01, 0x06, 0x11, 0x07, 0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00,
                                          0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x15, 0x12, 0x00, 0x00, 0x03,
                                          0x09, 0x54, 0x65, 0x73, 0x74, 0x00, 0x00, 0x00, 0x00};

static void bench_ble_batch_old(std::vector<uint8_t> &buf) {
  buf.clear();
  OldProtoWriteBuffer writer(&buf);
  // Simulate encoding 8 BLE advertisements
  for (int i = 0; i < 8; i++) {
    // Each advertisement fields (flattened, no nested message for simplicity)
    writer.encode_uint32(1, static_cast<uint32_t>(0xAABBCCDD + i));  // address (lower 32)
    writer.encode_uint32(2, static_cast<uint32_t>(-70 + i));         // rssi
    writer.encode_uint32(3, 0);                                      // address_type (public)
    writer.encode_bytes(4, FAKE_BLE_DATA, sizeof(FAKE_BLE_DATA));    // data
  }
}

static size_t calc_ble_batch_size() {
  uint32_t size = 0;
  for (int i = 0; i < 8; i++) {
    size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(0xAABBCCDD + i));
    size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(-70 + i));
    size += ProtoSize::calc_uint32(1, 0);
    size += ProtoSize::calc_length(1, sizeof(FAKE_BLE_DATA));
  }
  return size;
}

static void bench_ble_batch_new(std::vector<uint8_t> &buf, size_t size) {
  buf.resize(size);
  NewProtoWriteBuffer writer(&buf, 0);
  for (int i = 0; i < 8; i++) {
    writer.encode_uint32(1, static_cast<uint32_t>(0xAABBCCDD + i));
    writer.encode_uint32(2, static_cast<uint32_t>(-70 + i));
    writer.encode_uint32(3, 0);
    writer.encode_bytes(4, FAKE_BLE_DATA, sizeof(FAKE_BLE_DATA));
  }
}

// ============================================================================
// Benchmark: Simulate SubscribeLogsResponse encoding
// This is a frequent message: level (enum/uint32) + message (bytes)
// Message sizes vary from short to long log lines.
// ============================================================================

static const char LOG_SHORT[] = "[sensor:042]: 'Temperature': Sending state 23.50 °C";
static const char LOG_LONG[] = "[wifi:042]: Connecting to 'MyNetwork'... [wifi:042]: Connected! "
                               "IP=192.168.1.100, SSID=MyNetwork, BSSID=AA:BB:CC:DD:EE:FF, Channel=6, RSSI=-42 dB";

static void bench_log_msg_old(std::vector<uint8_t> &buf) {
  buf.clear();
  OldProtoWriteBuffer writer(&buf);
  writer.encode_uint32(1, 3);  // level = DEBUG
  writer.encode_bytes(3, reinterpret_cast<const uint8_t *>(LOG_SHORT), strlen(LOG_SHORT));
}

static size_t calc_log_msg_size() {
  uint32_t size = 0;
  size += ProtoSize::calc_uint32(1, 3);
  size += ProtoSize::calc_length(1, strlen(LOG_SHORT));
  return size;
}

static void bench_log_msg_new(std::vector<uint8_t> &buf, size_t size) {
  buf.resize(size);
  NewProtoWriteBuffer writer(&buf, 0);
  writer.encode_uint32(1, 3);
  writer.encode_bytes(3, reinterpret_cast<const uint8_t *>(LOG_SHORT), strlen(LOG_SHORT));
}

static void bench_log_long_old(std::vector<uint8_t> &buf) {
  buf.clear();
  OldProtoWriteBuffer writer(&buf);
  writer.encode_uint32(1, 3);
  writer.encode_bytes(3, reinterpret_cast<const uint8_t *>(LOG_LONG), strlen(LOG_LONG));
}

static size_t calc_log_long_size() {
  uint32_t size = 0;
  size += ProtoSize::calc_uint32(1, 3);
  size += ProtoSize::calc_length(1, strlen(LOG_LONG));
  return size;
}

static void bench_log_long_new(std::vector<uint8_t> &buf, size_t size) {
  buf.resize(size);
  NewProtoWriteBuffer writer(&buf, 0);
  writer.encode_uint32(1, 3);
  writer.encode_bytes(3, reinterpret_cast<const uint8_t *>(LOG_LONG), strlen(LOG_LONG));
}

// ============================================================================
// Benchmark: Full encode cycle including calculate_size + resize + encode
// This measures the realistic overhead of the pre-sizing approach.
// ============================================================================

static void bench_full_cycle_sensor_old(std::vector<uint8_t> &buf) {
  // Old approach: just encode directly (vector grows as needed)
  buf.clear();
  buf.reserve(32);  // Typical small reserve
  OldProtoWriteBuffer writer(&buf);
  writer.encode_fixed32(1, 0x12345678);
  writer.encode_float(2, 23.5f);
  writer.encode_bool(3, false);
}

static void bench_full_cycle_sensor_new(std::vector<uint8_t> &buf) {
  // New approach: calculate size, resize, then encode
  uint32_t size = 0;
  size += ProtoSize::calc_fixed32(1, 0x12345678);
  size += ProtoSize::calc_float(1, 23.5f);
  size += ProtoSize::calc_bool(1, false);

  buf.clear();
  buf.resize(size);
  NewProtoWriteBuffer writer(&buf, 0);
  writer.encode_fixed32(1, 0x12345678);
  writer.encode_float(2, 23.5f);
  writer.encode_bool(3, false);
}

// ============================================================================
// Correctness verification
// ============================================================================

static bool verify_encoding_match() {
  std::vector<uint8_t> old_buf, new_buf;
  bool all_pass = true;

  auto check = [&](const char *name) {
    if (old_buf.size() != new_buf.size() || memcmp(old_buf.data(), new_buf.data(), old_buf.size()) != 0) {
      printf("FAIL: %s - output mismatch (old=%zu bytes, new=%zu bytes)\n", name, old_buf.size(), new_buf.size());
      all_pass = false;
    }
  };

  // Varint
  bench_varint_old(old_buf);
  bench_varint_new(new_buf, old_buf.size());
  check("varint");

  // Strings
  bench_strings_old(old_buf);
  bench_strings_new(new_buf, calc_strings_size());
  check("strings");

  // Fixed32
  bench_fixed32_old(old_buf);
  bench_fixed32_new(new_buf, calc_fixed32_size());
  check("fixed32");

  // SensorStateResponse
  bench_sensor_state_old(old_buf);
  bench_sensor_state_new(new_buf, calc_sensor_state_size());
  check("sensor_state");

  // ListEntitiesSensorResponse
  bench_list_entities_old(old_buf);
  bench_list_entities_new(new_buf, calc_list_entities_size());
  check("list_entities");

  // BLE batch
  bench_ble_batch_old(old_buf);
  bench_ble_batch_new(new_buf, calc_ble_batch_size());
  check("ble_batch");

  // Log message
  bench_log_msg_old(old_buf);
  bench_log_msg_new(new_buf, calc_log_msg_size());
  check("log_short");

  // Long log message
  bench_log_long_old(old_buf);
  bench_log_long_new(new_buf, calc_log_long_size());
  check("log_long");

  return all_pass;
}

// ============================================================================
// Main
// ============================================================================

int main() {
  printf("=== ProtoWriteBuffer Encoding Benchmark ===\n");
  printf("Comparing push_back() vs pre-sized pointer writes\n\n");

  // Verify correctness first
  printf("--- Correctness Verification ---\n");
  if (!verify_encoding_match()) {
    printf("CORRECTNESS CHECK FAILED - encoding output differs!\n");
    return 1;
  }
  printf("All encoding outputs match between old and new implementations.\n\n");

  // Calculate sizes for pre-allocation
  size_t varint_size = 1 + 2 + 3 + 4 + 5;  // 15 bytes
  size_t strings_size = calc_strings_size();
  size_t fixed32_size = calc_fixed32_size();
  size_t sensor_state_size = calc_sensor_state_size();
  size_t list_entities_size = calc_list_entities_size();
  size_t ble_batch_size = calc_ble_batch_size();
  size_t log_msg_size = calc_log_msg_size();
  size_t log_long_size = calc_log_long_size();

  std::vector<uint8_t> buf;
  buf.reserve(1024);  // Pre-allocate to avoid measuring allocation

  std::vector<BenchResult> results;

  // --- Varint encoding ---
  printf("--- Running Benchmarks ---\n\n");

  results.push_back(benchmark("varint_mix (old/push_back)", varint_size, [&] { bench_varint_old(buf); }));
  results.push_back(benchmark("varint_mix (new/pointer)", varint_size, [&] { bench_varint_new(buf, varint_size); }));

  // --- String encoding ---
  results.push_back(benchmark("strings_mix (old/push_back)", strings_size, [&] { bench_strings_old(buf); }));
  results.push_back(
      benchmark("strings_mix (new/pointer)", strings_size, [&] { bench_strings_new(buf, strings_size); }));

  // --- Fixed32 encoding ---
  results.push_back(benchmark("fixed32_x10 (old/push_back)", fixed32_size, [&] { bench_fixed32_old(buf); }));
  results.push_back(
      benchmark("fixed32_x10 (new/pointer)", fixed32_size, [&] { bench_fixed32_new(buf, fixed32_size); }));

  // --- SensorStateResponse ---
  results.push_back(benchmark("sensor_state (old/push_back)", sensor_state_size, [&] { bench_sensor_state_old(buf); }));
  results.push_back(benchmark("sensor_state (new/pointer)", sensor_state_size,
                              [&] { bench_sensor_state_new(buf, sensor_state_size); }));

  // --- ListEntitiesSensorResponse ---
  results.push_back(
      benchmark("list_entities (old/push_back)", list_entities_size, [&] { bench_list_entities_old(buf); }));
  results.push_back(benchmark("list_entities (new/pointer)", list_entities_size,
                              [&] { bench_list_entities_new(buf, list_entities_size); }));

  // --- BLE batch ---
  results.push_back(benchmark("ble_batch_x8 (old/push_back)", ble_batch_size, [&] { bench_ble_batch_old(buf); }));
  results.push_back(
      benchmark("ble_batch_x8 (new/pointer)", ble_batch_size, [&] { bench_ble_batch_new(buf, ble_batch_size); }));

  // --- Log messages ---
  results.push_back(benchmark("log_short (old/push_back)", log_msg_size, [&] { bench_log_msg_old(buf); }));
  results.push_back(benchmark("log_short (new/pointer)", log_msg_size, [&] { bench_log_msg_new(buf, log_msg_size); }));

  results.push_back(benchmark("log_long (old/push_back)", log_long_size, [&] { bench_log_long_old(buf); }));
  results.push_back(
      benchmark("log_long (new/pointer)", log_long_size, [&] { bench_log_long_new(buf, log_long_size); }));

  // --- Full encode cycle (calculate_size + resize + encode) ---
  results.push_back(
      benchmark("full_cycle_sensor (old/push_back)", sensor_state_size, [&] { bench_full_cycle_sensor_old(buf); }));
  results.push_back(
      benchmark("full_cycle_sensor (new/pointer)", sensor_state_size, [&] { bench_full_cycle_sensor_new(buf); }));

  // Print all results
  printf("\n--- Results ---\n\n");
  print_results(results);

  // Print comparison summary
  printf("\n--- Speedup Summary (new vs old) ---\n\n");
  for (size_t i = 0; i + 1 < results.size(); i += 2) {
    print_comparison(results[i].name, results[i], results[i + 1]);
  }

  printf("\n--- Encoded Sizes ---\n\n");
  printf("  varint_mix:       %3zu bytes\n", varint_size);
  printf("  strings_mix:      %3zu bytes\n", strings_size);
  printf("  fixed32_x10:      %3zu bytes\n", fixed32_size);
  printf("  sensor_state:     %3zu bytes\n", sensor_state_size);
  printf("  list_entities:    %3zu bytes\n", list_entities_size);
  printf("  ble_batch_x8:     %3zu bytes\n", ble_batch_size);
  printf("  log_short:        %3zu bytes\n", log_msg_size);
  printf("  log_long:         %3zu bytes\n", log_long_size);

  return 0;
}
