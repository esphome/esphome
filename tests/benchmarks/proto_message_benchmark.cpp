/**
 * Benchmark: Virtual dispatch vs direct calls for protobuf message encoding
 *
 * Compares:
 *   OLD: virtual dispatch for encode/calculate_size + ProtoSize accumulator object
 *   NEW: direct template calls for encode/calculate_size + static ProtoSize methods
 *
 * Build (from repo root):
 *   g++ -std=gnu++20 -O2 \
 *       tests/benchmarks/proto_message_benchmark.cpp \
 *       -o tests/benchmarks/proto_message_benchmark
 *
 * Run:
 *   ./tests/benchmarks/proto_message_benchmark
 */

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <string>
#include <vector>

// ============================================================================
// Benchmark infrastructure
// ============================================================================

struct BenchResult {
  const char *name;
  double ns_per_op;
  double ops_per_sec;
  size_t iterations;
};

template<typename T> __attribute__((noinline)) void do_not_optimize(T &value) {
  asm volatile("" : "+r,m"(value) : : "memory");
}

__attribute__((noinline)) void clobber_memory() { asm volatile("" : : : "memory"); }

template<typename Func> BenchResult benchmark(const char *name, Func func) {
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

  // Scale iterations to target ~500ms for stability
  iterations = std::max<size_t>(100000, static_cast<size_t>(500'000'000.0 / ns_per_op));

  // Actual benchmark run
  start = std::chrono::high_resolution_clock::now();
  for (size_t i = 0; i < iterations; i++) {
    func();
    clobber_memory();
  }
  end = std::chrono::high_resolution_clock::now();
  elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  ns_per_op = elapsed_ns / iterations;

  return BenchResult{name, ns_per_op, 1'000'000'000.0 / ns_per_op, iterations};
}

void print_results(const std::vector<BenchResult> &results) {
  printf("%-55s %12s %15s %12s\n", "Benchmark", "ns/op", "ops/sec", "iters");
  printf("%-55s %12s %15s %12s\n", std::string(55, '-').c_str(), "--------", "--------", "--------");
  for (const auto &r : results) {
    printf("%-55s %12.1f %15.0f %12zu\n", r.name, r.ns_per_op, r.ops_per_sec, r.iterations);
  }
}

void print_comparison(const char *label, const BenchResult &old_result, const BenchResult &new_result) {
  double speedup = old_result.ns_per_op / new_result.ns_per_op;
  const char *dir = speedup > 1.0 ? "faster" : "slower";
  printf("  %-51s  %5.2fx %s\n", label, speedup > 1.0 ? speedup : 1.0 / speedup, dir);
}

// ============================================================================
// Shared encoding helpers (same for both old and new)
// ============================================================================

static constexpr uint32_t varint_size(uint32_t value) {
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

class WriteBuffer {
 public:
  WriteBuffer(std::vector<uint8_t> *buffer, size_t write_pos) : buffer_(buffer), pos_(buffer->data() + write_pos) {}

  void encode_varint_raw(uint32_t value) {
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
    std::memcpy(this->pos_, &value, 4);
    this->pos_ += 4;
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

  // Nested message encoding (for old-style virtual dispatch)
  void encode_message_virtual(uint32_t field_id, uint32_t nested_size, const void *value,
                              void (*encode_fn)(const void *, WriteBuffer &), bool force) {
    if (nested_size == 0 && !force)
      return;
    this->encode_field_raw(field_id, 2);
    this->encode_varint_raw(nested_size);
    encode_fn(value, *this);
  }

  // Nested message encoding (for new-style direct calls)
  template<typename T> void encode_message(uint32_t field_id, const T &value, bool force = true) {
    uint32_t nested_size = value.calculate_size();
    if (nested_size == 0 && !force)
      return;
    this->encode_field_raw(field_id, 2);
    this->encode_varint_raw(nested_size);
    value.encode(*this);
  }

  std::vector<uint8_t> *buffer_;
  uint8_t *pos_;
};

// ============================================================================
// OLD approach: ProtoSize accumulator + virtual dispatch
// ============================================================================

namespace old_style {

class ProtoSize {
 public:
  ProtoSize() = default;
  uint32_t get_size() const { return total_size_; }

  void add_uint32(uint32_t field_id_size, uint32_t value) {
    if (value != 0)
      total_size_ += field_id_size + varint_size(value);
  }
  void add_bool(uint32_t field_id_size, bool value) {
    if (value)
      total_size_ += field_id_size + 1;
  }
  void add_float(uint32_t field_id_size, float value) {
    if (value != 0.0f)
      total_size_ += field_id_size + 4;
  }
  void add_fixed32(uint32_t field_id_size, uint32_t value) {
    if (value != 0)
      total_size_ += field_id_size + 4;
  }
  void add_length(uint32_t field_id_size, size_t len) {
    if (len != 0)
      total_size_ += field_id_size + varint_size(static_cast<uint32_t>(len)) + static_cast<uint32_t>(len);
  }
  void add_message_field_force(uint32_t field_id_size, uint32_t nested_size) {
    total_size_ += field_id_size + varint_size(nested_size) + nested_size;
  }

 private:
  uint32_t total_size_ = 0;
};

class ProtoMessage {
 public:
  virtual void encode(WriteBuffer &buffer) const = 0;
  virtual uint32_t calculate_size() const = 0;
  virtual ~ProtoMessage() = default;
};

// Empty message (ping, disconnect, etc.)
class EmptyMessage : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 1;
  void encode(WriteBuffer &buffer) const override {}
  uint32_t calculate_size() const override { return 0; }
};

// SensorStateResponse: fixed32 key, float state, bool missing_state
class SensorStateResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 25;
  uint32_t key{0x12345678};
  float state{23.5f};
  bool missing_state{false};

  void encode(WriteBuffer &buffer) const override {
    buffer.encode_fixed32(1, this->key);
    buffer.encode_float(2, this->state);
    buffer.encode_bool(3, this->missing_state);
  }

  uint32_t calculate_size() const override {
    ProtoSize size;
    size.add_fixed32(1, this->key);
    size.add_float(1, this->state);
    size.add_bool(1, this->missing_state);
    return size.get_size();
  }
};

// ListEntitiesSensorResponse: multiple strings + numeric fields
class ListEntitiesSensorResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 16;
  std::string object_id{"living_room_temp"};
  uint32_t key{0xABCD1234};
  std::string name{"Living Room Temperature"};
  std::string unique_id{"esp32_01-sensor-living_room_temp"};
  std::string icon{"mdi:thermometer"};
  std::string unit_of_measurement{"\xc2\xb0"
                                  "C"};
  uint32_t accuracy_decimals{1};
  bool force_update{false};
  std::string device_class{"temperature"};
  uint32_t state_class{1};

  void encode(WriteBuffer &buffer) const override {
    buffer.encode_string(1, this->object_id.data(), this->object_id.size());
    buffer.encode_fixed32(2, this->key);
    buffer.encode_string(3, this->name.data(), this->name.size());
    buffer.encode_string(4, this->unique_id.data(), this->unique_id.size());
    buffer.encode_string(5, this->icon.data(), this->icon.size());
    buffer.encode_string(6, this->unit_of_measurement.data(), this->unit_of_measurement.size());
    buffer.encode_uint32(7, this->accuracy_decimals);
    buffer.encode_bool(8, this->force_update);
    buffer.encode_string(9, this->device_class.data(), this->device_class.size());
    buffer.encode_uint32(10, this->state_class);
  }

  uint32_t calculate_size() const override {
    ProtoSize size;
    size.add_length(1, this->object_id.size());
    size.add_fixed32(1, this->key);
    size.add_length(1, this->name.size());
    size.add_length(1, this->unique_id.size());
    size.add_length(1, this->icon.size());
    size.add_length(1, this->unit_of_measurement.size());
    size.add_uint32(1, this->accuracy_decimals);
    size.add_bool(1, this->force_update);
    size.add_length(1, this->device_class.size());
    size.add_uint32(1, this->state_class);
    return size.get_size();
  }
};

// SubscribeLogsResponse: level + message bytes
class SubscribeLogsResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 29;
  uint32_t level{3};
  std::string message{"[sensor:042]: 'Temperature': Sending state 23.50 C with 1 decimals of accuracy"};

  void encode(WriteBuffer &buffer) const override {
    buffer.encode_uint32(1, this->level);
    buffer.encode_bytes(3, reinterpret_cast<const uint8_t *>(this->message.data()), this->message.size());
  }

  uint32_t calculate_size() const override {
    ProtoSize size;
    size.add_uint32(1, this->level);
    size.add_length(1, this->message.size());
    return size.get_size();
  }
};

// Nested message: BluetoothGATTService with characteristics
class BluetoothGATTCharacteristic : public ProtoMessage {
 public:
  uint32_t uuid1{0x2A19};
  uint32_t handle{3};
  uint32_t properties{2};

  void encode(WriteBuffer &buffer) const override {
    buffer.encode_uint32(1, this->uuid1);
    buffer.encode_uint32(2, this->handle);
    buffer.encode_uint32(3, this->properties);
  }

  uint32_t calculate_size() const override {
    ProtoSize size;
    size.add_uint32(1, this->uuid1);
    size.add_uint32(1, this->handle);
    size.add_uint32(1, this->properties);
    return size.get_size();
  }
};

class BluetoothGATTService : public ProtoMessage {
 public:
  uint32_t uuid1{0x180F};
  uint32_t handle{1};
  std::vector<BluetoothGATTCharacteristic> characteristics;

  BluetoothGATTService() { characteristics.resize(4); }

  void encode(WriteBuffer &buffer) const override {
    buffer.encode_uint32(1, this->uuid1);
    buffer.encode_uint32(2, this->handle);
    for (const auto &ch : this->characteristics) {
      buffer.encode_message_virtual(
          3, ch.calculate_size(), &ch,
          [](const void *msg, WriteBuffer &buf) { static_cast<const BluetoothGATTCharacteristic *>(msg)->encode(buf); },
          true);
    }
  }

  uint32_t calculate_size() const override {
    ProtoSize size;
    size.add_uint32(1, this->uuid1);
    size.add_uint32(1, this->handle);
    for (const auto &ch : this->characteristics) {
      size.add_message_field_force(1, ch.calculate_size());
    }
    return size.get_size();
  }
};

// send_message simulation: virtual dispatch through base pointer
__attribute__((noinline)) bool send_message(const ProtoMessage &msg, uint8_t msg_type, std::vector<uint8_t> &buf) {
  uint32_t size = msg.calculate_size();
  buf.resize(size);
  WriteBuffer writer(&buf, 0);
  msg.encode(writer);
  do_not_optimize(buf);
  return true;
}

}  // namespace old_style

// ============================================================================
// NEW approach: static ProtoSize + direct template calls
// ============================================================================

namespace new_style {

class ProtoSize {
 public:
  static constexpr uint32_t calc_uint32(uint32_t field_id_size, uint32_t value) {
    return value ? field_id_size + varint_size(value) : 0;
  }
  static constexpr uint32_t calc_bool(uint32_t field_id_size, bool value) { return value ? field_id_size + 1 : 0; }
  static constexpr uint32_t calc_float(uint32_t field_id_size, float value) {
    return value != 0.0f ? field_id_size + 4 : 0;
  }
  static constexpr uint32_t calc_fixed32(uint32_t field_id_size, uint32_t value) {
    return value ? field_id_size + 4 : 0;
  }
  static constexpr uint32_t calc_length(uint32_t field_id_size, size_t len) {
    return len ? field_id_size + varint_size(static_cast<uint32_t>(len)) + static_cast<uint32_t>(len) : 0;
  }
  static constexpr uint32_t calc_message_force(uint32_t field_id_size, uint32_t nested_size) {
    return field_id_size + varint_size(nested_size) + nested_size;
  }
};

class ProtoMessage {
 public:
  // Non-virtual defaults — concrete types hide these
  void encode(WriteBuffer &buffer) const {}
  uint32_t calculate_size() const { return 0; }
  ~ProtoMessage() = default;
};

// Empty message
class EmptyMessage : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 1;
  static constexpr uint32_t ESTIMATED_SIZE = 0;
  void encode(WriteBuffer &buffer) const {}
  uint32_t calculate_size() const { return 0; }
};

// SensorStateResponse
class SensorStateResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 25;
  static constexpr uint32_t ESTIMATED_SIZE = 10;
  uint32_t key{0x12345678};
  float state{23.5f};
  bool missing_state{false};

  void encode(WriteBuffer &buffer) const {
    buffer.encode_fixed32(1, this->key);
    buffer.encode_float(2, this->state);
    buffer.encode_bool(3, this->missing_state);
  }

  uint32_t calculate_size() const {
    uint32_t size = 0;
    size += ProtoSize::calc_fixed32(1, this->key);
    size += ProtoSize::calc_float(1, this->state);
    size += ProtoSize::calc_bool(1, this->missing_state);
    return size;
  }
};

// ListEntitiesSensorResponse
class ListEntitiesSensorResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 16;
  static constexpr uint32_t ESTIMATED_SIZE = 128;
  std::string object_id{"living_room_temp"};
  uint32_t key{0xABCD1234};
  std::string name{"Living Room Temperature"};
  std::string unique_id{"esp32_01-sensor-living_room_temp"};
  std::string icon{"mdi:thermometer"};
  std::string unit_of_measurement{"\xc2\xb0"
                                  "C"};
  uint32_t accuracy_decimals{1};
  bool force_update{false};
  std::string device_class{"temperature"};
  uint32_t state_class{1};

  void encode(WriteBuffer &buffer) const {
    buffer.encode_string(1, this->object_id.data(), this->object_id.size());
    buffer.encode_fixed32(2, this->key);
    buffer.encode_string(3, this->name.data(), this->name.size());
    buffer.encode_string(4, this->unique_id.data(), this->unique_id.size());
    buffer.encode_string(5, this->icon.data(), this->icon.size());
    buffer.encode_string(6, this->unit_of_measurement.data(), this->unit_of_measurement.size());
    buffer.encode_uint32(7, this->accuracy_decimals);
    buffer.encode_bool(8, this->force_update);
    buffer.encode_string(9, this->device_class.data(), this->device_class.size());
    buffer.encode_uint32(10, this->state_class);
  }

  uint32_t calculate_size() const {
    uint32_t size = 0;
    size += ProtoSize::calc_length(1, this->object_id.size());
    size += ProtoSize::calc_fixed32(1, this->key);
    size += ProtoSize::calc_length(1, this->name.size());
    size += ProtoSize::calc_length(1, this->unique_id.size());
    size += ProtoSize::calc_length(1, this->icon.size());
    size += ProtoSize::calc_length(1, this->unit_of_measurement.size());
    size += ProtoSize::calc_uint32(1, this->accuracy_decimals);
    size += ProtoSize::calc_bool(1, this->force_update);
    size += ProtoSize::calc_length(1, this->device_class.size());
    size += ProtoSize::calc_uint32(1, this->state_class);
    return size;
  }
};

// SubscribeLogsResponse
class SubscribeLogsResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 29;
  static constexpr uint32_t ESTIMATED_SIZE = 80;
  uint32_t level{3};
  std::string message{"[sensor:042]: 'Temperature': Sending state 23.50 C with 1 decimals of accuracy"};

  void encode(WriteBuffer &buffer) const {
    buffer.encode_uint32(1, this->level);
    buffer.encode_bytes(3, reinterpret_cast<const uint8_t *>(this->message.data()), this->message.size());
  }

  uint32_t calculate_size() const {
    uint32_t size = 0;
    size += ProtoSize::calc_uint32(1, this->level);
    size += ProtoSize::calc_length(1, this->message.size());
    return size;
  }
};

// Nested: BluetoothGATTCharacteristic
class BluetoothGATTCharacteristic : public ProtoMessage {
 public:
  static constexpr uint32_t ESTIMATED_SIZE = 10;
  uint32_t uuid1{0x2A19};
  uint32_t handle{3};
  uint32_t properties{2};

  void encode(WriteBuffer &buffer) const {
    buffer.encode_uint32(1, this->uuid1);
    buffer.encode_uint32(2, this->handle);
    buffer.encode_uint32(3, this->properties);
  }

  uint32_t calculate_size() const {
    uint32_t size = 0;
    size += ProtoSize::calc_uint32(1, this->uuid1);
    size += ProtoSize::calc_uint32(1, this->handle);
    size += ProtoSize::calc_uint32(1, this->properties);
    return size;
  }
};

// Nested: BluetoothGATTService
class BluetoothGATTService : public ProtoMessage {
 public:
  static constexpr uint32_t ESTIMATED_SIZE = 64;
  uint32_t uuid1{0x180F};
  uint32_t handle{1};
  std::vector<BluetoothGATTCharacteristic> characteristics;

  BluetoothGATTService() { characteristics.resize(4); }

  void encode(WriteBuffer &buffer) const {
    buffer.encode_uint32(1, this->uuid1);
    buffer.encode_uint32(2, this->handle);
    for (const auto &ch : this->characteristics) {
      buffer.encode_message(3, ch, true);
    }
  }

  uint32_t calculate_size() const {
    uint32_t size = 0;
    size += ProtoSize::calc_uint32(1, this->uuid1);
    size += ProtoSize::calc_uint32(1, this->handle);
    for (const auto &ch : this->characteristics) {
      size += ProtoSize::calc_message_force(1, ch.calculate_size());
    }
    return size;
  }
};

// Encode thunk for non-template core
template<typename T> void encode_msg(const void *msg, WriteBuffer &buf) { static_cast<const T *>(msg)->encode(buf); }

static void encode_msg_noop(const void *, WriteBuffer &) {}

// send_message template: direct calls, no virtual dispatch
template<typename T> __attribute__((noinline)) bool send_message(const T &msg, std::vector<uint8_t> &buf) {
  uint32_t size;
  void (*encode_fn)(const void *, WriteBuffer &);
  if constexpr (T::ESTIMATED_SIZE == 0) {
    size = 0;
    encode_fn = &encode_msg_noop;
  } else {
    size = msg.calculate_size();
    encode_fn = &encode_msg<T>;
  }
  buf.resize(size);
  WriteBuffer writer(&buf, 0);
  encode_fn(&msg, writer);
  do_not_optimize(buf);
  return true;
}

}  // namespace new_style

// ============================================================================
// Correctness verification
// ============================================================================

static bool verify_correctness() {
  std::vector<uint8_t> old_buf, new_buf;
  bool all_pass = true;

  auto check = [&](const char *name) {
    if (old_buf.size() != new_buf.size() ||
        (old_buf.size() > 0 && memcmp(old_buf.data(), new_buf.data(), old_buf.size()) != 0)) {
      printf("FAIL: %s - output mismatch (old=%zu bytes, new=%zu bytes)\n", name, old_buf.size(), new_buf.size());
      all_pass = false;
    } else {
      printf("  OK: %s (%zu bytes)\n", name, old_buf.size());
    }
  };

  // Empty
  {
    old_style::EmptyMessage old_msg;
    new_style::EmptyMessage new_msg;
    old_style::send_message(old_msg, old_msg.MESSAGE_TYPE, old_buf);
    new_style::send_message(new_msg, new_buf);
    check("EmptyMessage");
  }
  // SensorState
  {
    old_style::SensorStateResponse old_msg;
    new_style::SensorStateResponse new_msg;
    old_style::send_message(old_msg, old_msg.MESSAGE_TYPE, old_buf);
    new_style::send_message(new_msg, new_buf);
    check("SensorStateResponse");
  }
  // ListEntities
  {
    old_style::ListEntitiesSensorResponse old_msg;
    new_style::ListEntitiesSensorResponse new_msg;
    old_style::send_message(old_msg, old_msg.MESSAGE_TYPE, old_buf);
    new_style::send_message(new_msg, new_buf);
    check("ListEntitiesSensorResponse");
  }
  // Log
  {
    old_style::SubscribeLogsResponse old_msg;
    new_style::SubscribeLogsResponse new_msg;
    old_style::send_message(old_msg, old_msg.MESSAGE_TYPE, old_buf);
    new_style::send_message(new_msg, new_buf);
    check("SubscribeLogsResponse");
  }
  // Nested (GATT service)
  {
    old_style::BluetoothGATTService old_msg;
    new_style::BluetoothGATTService new_msg;
    old_style::send_message(old_msg, 7, old_buf);
    new_style::send_message(new_msg, new_buf);
    check("BluetoothGATTService (nested)");
  }

  return all_pass;
}

// ============================================================================
// Benchmark: calculate_size only
// ============================================================================

template<typename T> __attribute__((noinline)) uint32_t bench_calc_size_virtual(const T &msg) {
  // Force virtual dispatch by going through base pointer
  const old_style::ProtoMessage *base = &msg;
  uint32_t s = base->calculate_size();
  do_not_optimize(s);
  return s;
}

template<typename T> __attribute__((noinline)) uint32_t bench_calc_size_direct(const T &msg) {
  uint32_t s = msg.calculate_size();
  do_not_optimize(s);
  return s;
}

// ============================================================================
// Main
// ============================================================================

int main() {
  printf("=== Proto Message Encoding Benchmark ===\n");
  printf("Comparing virtual dispatch + accumulator ProtoSize vs direct calls + static ProtoSize\n\n");

  // Verify correctness
  printf("--- Correctness Verification ---\n");
  if (!verify_correctness()) {
    printf("\nCORRECTNESS CHECK FAILED!\n");
    return 1;
  }
  printf("All outputs match.\n\n");

  std::vector<uint8_t> buf;
  buf.reserve(1024);

  std::vector<BenchResult> results;

  // ---- calculate_size benchmarks ----
  printf("--- Running calculate_size Benchmarks ---\n\n");

  {
    old_style::SensorStateResponse old_msg;
    new_style::SensorStateResponse new_msg;
    results.push_back(benchmark("calc_size: SensorState (virtual)", [&] { bench_calc_size_virtual(old_msg); }));
    results.push_back(benchmark("calc_size: SensorState (direct+static)", [&] { bench_calc_size_direct(new_msg); }));
  }
  {
    old_style::ListEntitiesSensorResponse old_msg;
    new_style::ListEntitiesSensorResponse new_msg;
    results.push_back(benchmark("calc_size: ListEntities (virtual)", [&] { bench_calc_size_virtual(old_msg); }));
    results.push_back(benchmark("calc_size: ListEntities (direct+static)", [&] { bench_calc_size_direct(new_msg); }));
  }
  {
    old_style::SubscribeLogsResponse old_msg;
    new_style::SubscribeLogsResponse new_msg;
    results.push_back(benchmark("calc_size: LogResponse (virtual)", [&] { bench_calc_size_virtual(old_msg); }));
    results.push_back(benchmark("calc_size: LogResponse (direct+static)", [&] { bench_calc_size_direct(new_msg); }));
  }
  {
    old_style::BluetoothGATTService old_msg;
    new_style::BluetoothGATTService new_msg;
    results.push_back(benchmark("calc_size: GATTService/nested (virtual)", [&] { bench_calc_size_virtual(old_msg); }));
    results.push_back(
        benchmark("calc_size: GATTService/nested (direct+static)", [&] { bench_calc_size_direct(new_msg); }));
  }

  // ---- Full send_message benchmarks ----
  printf("--- Running send_message Benchmarks ---\n\n");

  {
    old_style::EmptyMessage old_msg;
    new_style::EmptyMessage new_msg;
    results.push_back(benchmark("send: EmptyMessage (virtual)", [&] { old_style::send_message(old_msg, 1, buf); }));
    results.push_back(benchmark("send: EmptyMessage (direct+static)", [&] { new_style::send_message(new_msg, buf); }));
  }
  {
    old_style::SensorStateResponse old_msg;
    new_style::SensorStateResponse new_msg;
    results.push_back(benchmark("send: SensorState (virtual)", [&] { old_style::send_message(old_msg, 25, buf); }));
    results.push_back(benchmark("send: SensorState (direct+static)", [&] { new_style::send_message(new_msg, buf); }));
  }
  {
    old_style::ListEntitiesSensorResponse old_msg;
    new_style::ListEntitiesSensorResponse new_msg;
    results.push_back(benchmark("send: ListEntities (virtual)", [&] { old_style::send_message(old_msg, 16, buf); }));
    results.push_back(benchmark("send: ListEntities (direct+static)", [&] { new_style::send_message(new_msg, buf); }));
  }
  {
    old_style::SubscribeLogsResponse old_msg;
    new_style::SubscribeLogsResponse new_msg;
    results.push_back(benchmark("send: LogResponse (virtual)", [&] { old_style::send_message(old_msg, 29, buf); }));
    results.push_back(benchmark("send: LogResponse (direct+static)", [&] { new_style::send_message(new_msg, buf); }));
  }
  {
    old_style::BluetoothGATTService old_msg;
    new_style::BluetoothGATTService new_msg;
    results.push_back(
        benchmark("send: GATTService/nested (virtual)", [&] { old_style::send_message(old_msg, 7, buf); }));
    results.push_back(
        benchmark("send: GATTService/nested (direct+static)", [&] { new_style::send_message(new_msg, buf); }));
  }

  // Print all results
  printf("\n--- Results ---\n\n");
  print_results(results);

  // Print comparison summary
  printf("\n--- Speedup Summary (new vs old) ---\n\n");
  for (size_t i = 0; i + 1 < results.size(); i += 2) {
    print_comparison(results[i].name, results[i], results[i + 1]);
  }

  return 0;
}
