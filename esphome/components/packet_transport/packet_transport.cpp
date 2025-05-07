#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "packet_transport.h"

#include "esphome/components/xxtea/xxtea.h"

namespace esphome {
namespace packet_transport {
/**
 * Structure of a data packet; everything is little-endian
 *
 * --- In clear text ---
 * MAGIC_NUMBER: 16 bits
 * host name length: 1 byte
 * host name: (length) bytes
 * padding: 0 or more null bytes to a 4 byte boundary
 *
 * --- Encrypted (if key set) ----
 * DATA_KEY: 1 byte: OR ROLLING_CODE_KEY:
 *  Rolling code (if enabled): 8 bytes
 * Ping keys: if any
 * repeat:
 *      PING_KEY: 1 byte
 *      ping code: 4 bytes
 * Sensors:
 * repeat:
 *      SENSOR_KEY: 1 byte
 *      float value: 4 bytes
 *      name length: 1 byte
 *      name
 * Binary Sensors:
 * repeat:
 *      BINARY_SENSOR_KEY: 1 byte
 *      bool value: 1 bytes
 *      name length: 1 byte
 *      name
 *
 * Padded to a 4 byte boundary with nulls
 *
 * Structure of a ping request packet:
 * --- In clear text ---
 * MAGIC_PING: 16 bits
 * host name length: 1 byte
 * host name: (length) bytes
 * Ping key (4 bytes)
 *
 */
static const char *const TAG = "packet_transport";

static size_t round4(size_t value) { return (value + 3) & ~3; }

union FuData {
  uint32_t u32;
  float f32;
};

static const uint16_t MAGIC_NUMBER = 0x4553;
static const uint16_t MAGIC_PING = 0x5048;
static const uint32_t PREF_HASH = 0x45535043;
enum DataKey {
  ZERO_FILL_KEY,
  DATA_KEY,
  SENSOR_KEY,
  BINARY_SENSOR_KEY,
  PING_KEY,
  ROLLING_CODE_KEY,
};

enum DecodeResult {
  DECODE_OK,
  DECODE_UNMATCHED,
  DECODE_ERROR,
  DECODE_EMPTY,
};

static const size_t MAX_PING_KEYS = 4;

static inline void add(std::vector<uint8_t> &vec, uint32_t data) {
  vec.push_back(data & 0xFF);
  vec.push_back((data >> 8) & 0xFF);
  vec.push_back((data >> 16) & 0xFF);
  vec.push_back((data >> 24) & 0xFF);
}

class PacketDecoder {
 public:
  PacketDecoder(const uint8_t *buffer, size_t len) : buffer_(buffer), len_(len) {}

  DecodeResult decode_string(char *data, size_t maxlen) {
    if (this->position_ == this->len_)
      return DECODE_EMPTY;
    auto len = this->buffer_[this->position_];
    if (len == 0 || this->position_ + 1 + len > this->len_ || len >= maxlen)
      return DECODE_ERROR;
    this->position_++;
    memcpy(data, this->buffer_ + this->position_, len);
    data[len] = 0;
    this->position_ += len;
    return DECODE_OK;
  }

  template<typename T> DecodeResult get(T &data) {
    if (this->position_ + sizeof(T) > this->len_)
      return DECODE_ERROR;
    T value = 0;
    for (size_t i = 0; i != sizeof(T); ++i) {
      value += this->buffer_[this->position_++] << (i * 8);
    }
    data = value;
    return DECODE_OK;
  }

  template<typename T> DecodeResult decode(uint8_t key, T &data) {
    if (this->position_ == this->len_)
      return DECODE_EMPTY;
    if (this->buffer_[this->position_] != key)
      return DECODE_UNMATCHED;
    if (this->position_ + 1 + sizeof(T) > this->len_)
      return DECODE_ERROR;
    this->position_++;
    T value = 0;
    for (size_t i = 0; i != sizeof(T); ++i) {
      value += this->buffer_[this->position_++] << (i * 8);
    }
    data = value;
    return DECODE_OK;
  }

  template<typename T> DecodeResult decode(uint8_t key, char *buf, size_t buflen, T &data) {
    if (this->position_ == this->len_)
      return DECODE_EMPTY;
    if (this->buffer_[this->position_] != key)
      return DECODE_UNMATCHED;
    this->position_++;
    T value = 0;
    for (size_t i = 0; i != sizeof(T); ++i) {
      value += this->buffer_[this->position_++] << (i * 8);
    }
    data = value;
    return this->decode_string(buf, buflen);
  }

  DecodeResult decode(uint8_t key) {
    if (this->position_ == this->len_)
      return DECODE_EMPTY;
    if (this->buffer_[this->position_] != key)
      return DECODE_UNMATCHED;
    this->position_++;
    return DECODE_OK;
  }

  size_t get_remaining_size() const { return this->len_ - this->position_; }

  // align the pointer to the given byte boundary
  bool bump_to(size_t boundary) {
    auto newpos = this->position_;
    auto offset = this->position_ % boundary;
    if (offset != 0) {
      newpos += boundary - offset;
    }
    if (newpos >= this->len_)
      return false;
    this->position_ = newpos;
    return true;
  }

  bool decrypt(const uint32_t *key) {
    if (this->get_remaining_size() % 4 != 0) {
      return false;
    }
    xxtea::decrypt((uint32_t *) (this->buffer_ + this->position_), this->get_remaining_size() / 4, key);
    return true;
  }

 protected:
  const uint8_t *buffer_;
  size_t len_;
  size_t position_{};
};

static inline void add(std::vector<uint8_t> &vec, uint8_t data) { vec.push_back(data); }
static inline void add(std::vector<uint8_t> &vec, uint16_t data) {
  vec.push_back((uint8_t) data);
  vec.push_back((uint8_t) (data >> 8));
}
static inline void add(std::vector<uint8_t> &vec, DataKey data) { vec.push_back(data); }
static void add(std::vector<uint8_t> &vec, const char *str) {
  auto len = strlen(str);
  vec.push_back(len);
  for (size_t i = 0; i != len; i++) {
    vec.push_back(*str++);
  }
}

void PacketTransport::setup() {
  this->name_ = App.get_name().c_str();
  if (strlen(this->name_) > 255) {
    this->mark_failed();
    this->status_set_error("Device name exceeds 255 chars");
    return;
  }
  this->resend_ping_key_ = this->ping_pong_enable_;
  this->pref_ = global_preferences->make_preference<uint32_t>(PREF_HASH, true);
  if (this->rolling_code_enable_) {
    // restore the upper 32 bits of the rolling code, increment and save.
    this->pref_.load(&this->rolling_code_[1]);
    this->rolling_code_[1]++;
    this->pref_.save(&this->rolling_code_[1]);
    // must make sure it's saved immediately
    global_preferences->sync();
    this->ping_key_ = random_uint32();
    ESP_LOGV(TAG, "Rolling code incremented, upper part now %u", (unsigned) this->rolling_code_[1]);
  }
#ifdef USE_SENSOR
  for (auto &sensor : this->sensors_) {
    sensor.sensor->add_on_state_callback([this, &sensor](float x) {
      this->updated_ = true;
      sensor.updated = true;
    });
  }
#endif
#ifdef USE_BINARY_SENSOR
  for (auto &sensor : this->binary_sensors_) {
    sensor.sensor->add_on_state_callback([this, &sensor](bool value) {
      this->updated_ = true;
      sensor.updated = true;
    });
  }
#endif
  // initialise the header. This is invariant.
  add(this->header_, MAGIC_NUMBER);
  add(this->header_, this->name_);
  // pad to a multiple of 4 bytes
  while (this->header_.size() & 0x3)
    this->header_.push_back(0);
}

void PacketTransport::init_data_() {
  this->data_.clear();
  if (this->rolling_code_enable_) {
    add(this->data_, ROLLING_CODE_KEY);
    add(this->data_, this->rolling_code_[0]);
    add(this->data_, this->rolling_code_[1]);
    this->increment_code_();
  } else {
    add(this->data_, DATA_KEY);
  }
  for (auto pkey : this->ping_keys_) {
    add(this->data_, PING_KEY);
    add(this->data_, pkey.second);
  }
}

void PacketTransport::flush_() {
  if (!this->should_send() || this->data_.empty())
    return;
  auto header_len = round4(this->header_.size());
  auto len = round4(data_.size());
  auto encode_buffer = std::vector<uint8_t>(round4(header_len + len));
  memcpy(encode_buffer.data(), this->header_.data(), this->header_.size());
  memcpy(encode_buffer.data() + header_len, this->data_.data(), this->data_.size());
  if (this->is_encrypted_()) {
    xxtea::encrypt((uint32_t *) (encode_buffer.data() + header_len), len / 4,
                   (uint32_t *) this->encryption_key_.data());
  }
  this->send_packet(encode_buffer);
}

void PacketTransport::add_binary_data_(uint8_t key, const char *id, bool data) {
  auto len = 1 + 1 + 1 + strlen(id);
  if (len + this->header_.size() + this->data_.size() > this->get_max_packet_size()) {
    this->flush_();
  }
  add(this->data_, key);
  add(this->data_, (uint8_t) data);
  add(this->data_, id);
}
void PacketTransport::add_data_(uint8_t key, const char *id, float data) {
  FuData udata{.f32 = data};
  this->add_data_(key, id, udata.u32);
}

void PacketTransport::add_data_(uint8_t key, const char *id, uint32_t data) {
  auto len = 4 + 1 + 1 + strlen(id);
  if (len + this->header_.size() + this->data_.size() > this->get_max_packet_size()) {
    this->flush_();
  }
  add(this->data_, key);
  add(this->data_, data);
  add(this->data_, id);
}
void PacketTransport::send_data_(bool all) {
  if (!this->should_send())
    return;
  this->init_data_();
#ifdef USE_SENSOR
  for (auto &sensor : this->sensors_) {
    if (all || sensor.updated) {
      sensor.updated = false;
      this->add_data_(SENSOR_KEY, sensor.id, sensor.sensor->get_state());
    }
  }
#endif
#ifdef USE_BINARY_SENSOR
  for (auto &sensor : this->binary_sensors_) {
    if (all || sensor.updated) {
      sensor.updated = false;
      this->add_binary_data_(BINARY_SENSOR_KEY, sensor.id, sensor.sensor->state);
    }
  }
#endif
  this->flush_();
  this->updated_ = false;
}

void PacketTransport::update() {
  auto now = millis() / 1000;
  if (this->last_key_time_ + this->ping_pong_recyle_time_ < now) {
    this->resend_ping_key_ = this->ping_pong_enable_;
    this->last_key_time_ = now;
  }
}

void PacketTransport::add_key_(const char *name, uint32_t key) {
  if (!this->is_encrypted_())
    return;
  if (this->ping_keys_.count(name) == 0 && this->ping_keys_.size() == MAX_PING_KEYS) {
    ESP_LOGW(TAG, "Ping key from %s discarded", name);
    return;
  }
  this->ping_keys_[name] = key;
  this->updated_ = true;
  ESP_LOGV(TAG, "Ping key from %s now %X", name, (unsigned) key);
}

static bool process_rolling_code(Provider &provider, PacketDecoder &decoder) {
  uint32_t code0, code1;
  if (decoder.get(code0) != DECODE_OK || decoder.get(code1) != DECODE_OK) {
    ESP_LOGW(TAG, "Rolling code requires 8 bytes");
    return false;
  }
  if (code1 < provider.last_code[1] || (code1 == provider.last_code[1] && code0 <= provider.last_code[0])) {
    ESP_LOGW(TAG, "Rolling code for %s %08lX:%08lX is old", provider.name, (unsigned long) code1,
             (unsigned long) code0);
    return false;
  }
  provider.last_code[0] = code0;
  provider.last_code[1] = code1;
  ESP_LOGV(TAG, "Saw new rolling code for %s %08lX:%08lX", provider.name, (unsigned long) code1, (unsigned long) code0);
  return true;
}

/**
 * Process a received packet
 */
void PacketTransport::process_(const std::vector<uint8_t> &data) {
  auto ping_key_seen = !this->ping_pong_enable_;
  PacketDecoder decoder((data.data()), data.size());
  char namebuf[256]{};
  uint8_t byte;
  FuData rdata{};
  uint16_t magic;
  if (decoder.get(magic) != DECODE_OK) {
    ESP_LOGD(TAG, "Short buffer");
    return;
  }
  if (magic != MAGIC_NUMBER && magic != MAGIC_PING) {
    ESP_LOGV(TAG, "Bad magic %X", magic);
    return;
  }

  if (decoder.decode_string(namebuf, sizeof namebuf) != DECODE_OK) {
    ESP_LOGV(TAG, "Bad hostname length");
    return;
  }
  if (strcmp(this->name_, namebuf) == 0) {
    ESP_LOGVV(TAG, "Ignoring our own data");
    return;
  }
  if (magic == MAGIC_PING) {
    uint32_t key;
    if (decoder.get(key) != DECODE_OK) {
      ESP_LOGW(TAG, "Bad ping request");
      return;
    }
    this->add_key_(namebuf, key);
    ESP_LOGV(TAG, "Updated ping key for %s to %08X", namebuf, (unsigned) key);
    return;
  }

  if (this->providers_.count(namebuf) == 0) {
    ESP_LOGVV(TAG, "Unknown hostname %s", namebuf);
    return;
  }
  ESP_LOGV(TAG, "Found hostname %s", namebuf);

#ifdef USE_SENSOR
  auto &sensors = this->remote_sensors_[namebuf];
#endif
#ifdef USE_BINARY_SENSOR
  auto &binary_sensors = this->remote_binary_sensors_[namebuf];
#endif

  if (!decoder.bump_to(4)) {
    ESP_LOGW(TAG, "Bad packet length %zu", data.size());
  }
  auto len = decoder.get_remaining_size();
  if (round4(len) != len) {
    ESP_LOGW(TAG, "Bad payload length %zu", len);
    return;
  }

  auto &provider = this->providers_[namebuf];
  // if encryption not used with this host, ping check is pointless since it would be easily spoofed.
  if (provider.encryption_key.empty())
    ping_key_seen = true;

  if (!provider.encryption_key.empty()) {
    decoder.decrypt((const uint32_t *) provider.encryption_key.data());
  }
  if (decoder.get(byte) != DECODE_OK) {
    ESP_LOGV(TAG, "No key byte");
    return;
  }

  if (byte == ROLLING_CODE_KEY) {
    if (!process_rolling_code(provider, decoder))
      return;
  } else if (byte != DATA_KEY) {
    ESP_LOGV(TAG, "Expected rolling_key or data_key, got %X", byte);
    return;
  }
  uint32_t key;
  while (decoder.get_remaining_size() != 0) {
    if (decoder.decode(ZERO_FILL_KEY) == DECODE_OK)
      continue;
    if (decoder.decode(PING_KEY, key) == DECODE_OK) {
      if (key == this->ping_key_) {
        ping_key_seen = true;
        ESP_LOGV(TAG, "Found good ping key %X", (unsigned) key);
      } else {
        ESP_LOGV(TAG, "Unknown ping key %X", (unsigned) key);
      }
      continue;
    }
    if (!ping_key_seen) {
      ESP_LOGW(TAG, "Ping key not seen");
      this->resend_ping_key_ = true;
      break;
    }
    if (decoder.decode(BINARY_SENSOR_KEY, namebuf, sizeof(namebuf), byte) == DECODE_OK) {
      ESP_LOGV(TAG, "Got binary sensor %s %d", namebuf, byte);
#ifdef USE_BINARY_SENSOR
      if (binary_sensors.count(namebuf) != 0)
        binary_sensors[namebuf]->publish_state(byte != 0);
#endif
      continue;
    }
    if (decoder.decode(SENSOR_KEY, namebuf, sizeof(namebuf), rdata.u32) == DECODE_OK) {
      ESP_LOGV(TAG, "Got sensor %s %f", namebuf, rdata.f32);
#ifdef USE_SENSOR
      if (sensors.count(namebuf) != 0)
        sensors[namebuf]->publish_state(rdata.f32);
#endif
      continue;
    }
    if (decoder.get(byte) == DECODE_OK) {
      ESP_LOGW(TAG, "Unknown key %X", byte);
      ESP_LOGD(TAG, "Buffer pos: %zu contents: %s", data.size() - decoder.get_remaining_size(),
               format_hex_pretty(data).c_str());
    }
    break;
  }
}

void PacketTransport::dump_config() {
  ESP_LOGCONFIG(TAG, "Packet Transport:");
  ESP_LOGCONFIG(TAG, "  Platform: %s", this->platform_name_);
  ESP_LOGCONFIG(TAG, "  Encrypted: %s", YESNO(this->is_encrypted_()));
  ESP_LOGCONFIG(TAG, "  Ping-pong: %s", YESNO(this->ping_pong_enable_));
#ifdef USE_SENSOR
  for (auto sensor : this->sensors_)
    ESP_LOGCONFIG(TAG, "  Sensor: %s", sensor.id);
#endif
#ifdef USE_BINARY_SENSOR
  for (auto sensor : this->binary_sensors_)
    ESP_LOGCONFIG(TAG, "  Binary Sensor: %s", sensor.id);
#endif
  for (const auto &host : this->providers_) {
    ESP_LOGCONFIG(TAG, "  Remote host: %s", host.first.c_str());
    ESP_LOGCONFIG(TAG, "    Encrypted: %s", YESNO(!host.second.encryption_key.empty()));
#ifdef USE_SENSOR
    for (const auto &sensor : this->remote_sensors_[host.first.c_str()])
      ESP_LOGCONFIG(TAG, "    Sensor: %s", sensor.first.c_str());
#endif
#ifdef USE_BINARY_SENSOR
    for (const auto &sensor : this->remote_binary_sensors_[host.first.c_str()])
      ESP_LOGCONFIG(TAG, "    Binary Sensor: %s", sensor.first.c_str());
#endif
  }
}
void PacketTransport::increment_code_() {
  if (this->rolling_code_enable_) {
    if (++this->rolling_code_[0] == 0) {
      this->rolling_code_[1]++;
      this->pref_.save(&this->rolling_code_[1]);
      // must make sure it's saved immediately
      global_preferences->sync();
    }
  }
}

void PacketTransport::loop() {
  if (this->resend_ping_key_)
    this->send_ping_pong_request_();
  if (this->updated_) {
    this->send_data_(this->resend_data_);
  }
}

void PacketTransport::send_ping_pong_request_() {
  if (!this->ping_pong_enable_ || !this->should_send())
    return;
  this->ping_key_ = random_uint32();
  this->ping_header_.clear();
  add(this->ping_header_, MAGIC_PING);
  add(this->ping_header_, this->name_);
  add(this->ping_header_, this->ping_key_);
  this->send_packet(this->ping_header_);
  this->resend_ping_key_ = false;
  ESP_LOGV(TAG, "Sent new ping request %08X", (unsigned) this->ping_key_);
}
}  // namespace packet_transport
}  // namespace esphome
