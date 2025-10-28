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

static inline void add(uint8_t *buf, size_t &pos, uint32_t data) {
  buf[pos++] = data & 0xFF;
  buf[pos++] = (data >> 8) & 0xFF;
  buf[pos++] = (data >> 16) & 0xFF;
  buf[pos++] = (data >> 24) & 0xFF;
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

static inline void add(uint8_t *buf, size_t &pos, uint8_t data) { buf[pos++] = data; }
static inline void add(uint8_t *buf, size_t &pos, uint16_t data) {
  buf[pos++] = (uint8_t) data;
  buf[pos++] = (uint8_t) (data >> 8);
}
static inline void add(uint8_t *buf, size_t &pos, DataKey data) { buf[pos++] = data; }
static void add(uint8_t *buf, size_t &pos, const char *str) {
  auto len = strlen(str);
  buf[pos++] = len;
  for (size_t i = 0; i != len; i++) {
    buf[pos++] = *str++;
  }
}

void PacketTransport::setup() {
  this->name_ = App.get_name().c_str();
  if (strlen(this->name_) > 255) {
    this->mark_failed();
    this->status_set_error("Device name exceeds 255 chars");
    return;
  }
  this->providers_.count = 0;
  this->ping_key_count_ = 0;
#ifdef USE_SENSOR
  this->remote_sensor_count_ = 0;
#endif
#ifdef USE_BINARY_SENSOR
  this->remote_binary_sensor_count_ = 0;
#endif
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
  this->header_len_ = 0;
  add(this->header_, this->header_len_, MAGIC_NUMBER);
  add(this->header_, this->header_len_, this->name_);
  // pad to a multiple of 4 bytes
  while (this->header_len_ & 0x3)
    this->header_[this->header_len_++] = 0;
}

void PacketTransport::init_data_() {
  this->data_len_ = 0;
  if (this->rolling_code_enable_) {
    add(this->data_, this->data_len_, ROLLING_CODE_KEY);
    add(this->data_, this->data_len_, this->rolling_code_[0]);
    add(this->data_, this->data_len_, this->rolling_code_[1]);
    this->increment_code_();
  } else {
    add(this->data_, this->data_len_, DATA_KEY);
  }
  for (uint8_t i = 0; i < this->ping_key_count_; i++) {
    if (this->ping_keys_[i].active) {
      add(this->data_, this->data_len_, PING_KEY);
      add(this->data_, this->data_len_, this->ping_keys_[i].key);
    }
  }
}

void PacketTransport::flush_() {
  if (!this->should_send() || this->data_len_ == 0)
    return;
  auto header_len = round4(this->header_len_);
  auto len = round4(this->data_len_);
  auto total_len = header_len + len;
  std::vector<uint8_t> encode_buffer(total_len);
  memcpy(encode_buffer.data(), this->header_, this->header_len_);
  memset(encode_buffer.data() + this->header_len_, 0, header_len - this->header_len_);
  memcpy(encode_buffer.data() + header_len, this->data_, this->data_len_);
  memset(encode_buffer.data() + header_len + this->data_len_, 0, len - this->data_len_);
  if (this->is_encrypted_()) {
    xxtea::encrypt((uint32_t *) (encode_buffer.data() + header_len), len / 4, (uint32_t *) this->encryption_key_);
  }
  this->send_packet(encode_buffer);
}

void PacketTransport::add_binary_data_(uint8_t key, const char *id, bool data) {
  auto len = 1 + 1 + 1 + strlen(id);
  if (len + this->header_len_ + this->data_len_ > this->get_max_packet_size()) {
    this->flush_();
    this->init_data_();
  }
  add(this->data_, this->data_len_, key);
  add(this->data_, this->data_len_, (uint8_t) data);
  add(this->data_, this->data_len_, id);
}
void PacketTransport::add_data_(uint8_t key, const char *id, float data) {
  FuData udata{.f32 = data};
  this->add_data_(key, id, udata.u32);
}

void PacketTransport::add_data_(uint8_t key, const char *id, uint32_t data) {
  auto len = 4 + 1 + 1 + strlen(id);
  if (len + this->header_len_ + this->data_len_ > this->get_max_packet_size()) {
    this->flush_();
    this->init_data_();
  }
  add(this->data_, this->data_len_, key);
  add(this->data_, this->data_len_, data);
  add(this->data_, this->data_len_, id);
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
  if (!this->ping_pong_enable_) {
    return;
  }
  auto now = millis() / 1000;
  if (this->last_key_time_ + this->ping_pong_recyle_time_ < now) {
    this->resend_ping_key_ = this->ping_pong_enable_;
    ESP_LOGV(TAG, "Ping request, age %u", now - this->last_key_time_);
    this->last_key_time_ = now;
  }
  for (uint8_t i = 0; i < this->providers_.count; i++) {
    if (!this->providers_.data[i].active)
      continue;
    uint32_t key_response_age = now - this->providers_.data[i].last_key_response_time;
    if (key_response_age > (this->ping_pong_recyle_time_ * 2u)) {
#ifdef USE_STATUS_SENSOR
      if (this->providers_.data[i].status_sensor != nullptr && this->providers_.data[i].status_sensor->state) {
        ESP_LOGI(TAG, "Ping status for %s timeout at %u with age %u", this->providers_.data[i].name, now,
                 key_response_age);
        this->providers_.data[i].status_sensor->publish_state(false);
      }
#endif
#ifdef USE_SENSOR
      for (uint8_t j = 0; j < this->remote_sensor_count_; j++) {
        if (this->remote_sensors_[j].active && this->remote_sensors_[j].provider_index == i) {
          this->remote_sensors_[j].sensor->publish_state(NAN);
        }
      }
#endif
#ifdef USE_BINARY_SENSOR
      for (uint8_t j = 0; j < this->remote_binary_sensor_count_; j++) {
        if (this->remote_binary_sensors_[j].active && this->remote_binary_sensors_[j].provider_index == i) {
          this->remote_binary_sensors_[j].sensor->invalidate_state();
        }
      }
#endif
    } else {
#ifdef USE_STATUS_SENSOR
      if (this->providers_.data[i].status_sensor != nullptr && !this->providers_.data[i].status_sensor->state) {
        ESP_LOGI(TAG, "Ping status for %s restored at %u with age %u", this->providers_.data[i].name, now,
                 key_response_age);
        this->providers_.data[i].status_sensor->publish_state(true);
      }
#endif
    }
  }
}

void PacketTransport::add_key_(const char *name, uint32_t key) {
  if (!this->is_encrypted_())
    return;
  // Look for existing key for this name
  for (uint8_t i = 0; i < this->ping_key_count_; i++) {
    if (this->ping_keys_[i].active && strcmp(this->ping_keys_[i].name, name) == 0) {
      this->ping_keys_[i].key = key;
      this->updated_ = true;
      ESP_LOGV(TAG, "Ping key from %s now %X", name, (unsigned) key);
      return;
    }
  }
  // Add new key if space available
  if (this->ping_key_count_ < MAX_PING_KEYS) {
    this->ping_keys_[this->ping_key_count_].name = name;
    this->ping_keys_[this->ping_key_count_].key = key;
    this->ping_keys_[this->ping_key_count_].active = true;
    this->ping_key_count_++;
    this->updated_ = true;
    ESP_LOGV(TAG, "Ping key from %s now %X", name, (unsigned) key);
  } else {
    ESP_LOGW(TAG, "Ping key from %s discarded", name);
  }
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
  PacketDecoder decoder(data.data(), data.size());
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

  int8_t provider_index = this->find_provider_(namebuf);
  if (provider_index < 0) {
    ESP_LOGVV(TAG, "Unknown hostname %s", namebuf);
    return;
  }
  ESP_LOGV(TAG, "Found hostname %s", namebuf);

  if (!decoder.bump_to(4)) {
    ESP_LOGW(TAG, "Bad packet length %zu", data.size());
  }
  auto len = decoder.get_remaining_size();
  if (round4(len) != len) {
    ESP_LOGW(TAG, "Bad payload length %zu", len);
    return;
  }

  auto &provider = this->providers_.data[provider_index];
  // if encryption not used with this host, ping check is pointless since it would be easily spoofed.
  if (provider.key_length == 0)
    ping_key_seen = true;

  if (provider.key_length > 0) {
    decoder.decrypt((const uint32_t *) provider.encryption_key);
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
        provider.last_key_response_time = millis() / 1000;
        ESP_LOGV(TAG, "Found good ping key %X at timestamp %" PRIu32, (unsigned) key, provider.last_key_response_time);
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
      int8_t sensor_index = this->find_remote_binary_sensor_(provider_index, namebuf);
      if (sensor_index >= 0) {
        this->remote_binary_sensors_[sensor_index].sensor->publish_state(byte != 0);
      }
#endif
      continue;
    }
    if (decoder.decode(SENSOR_KEY, namebuf, sizeof(namebuf), rdata.u32) == DECODE_OK) {
      ESP_LOGV(TAG, "Got sensor %s %f", namebuf, rdata.f32);
#ifdef USE_SENSOR
      int8_t sensor_index = this->find_remote_sensor_(provider_index, namebuf);
      if (sensor_index >= 0) {
        this->remote_sensors_[sensor_index].sensor->publish_state(rdata.f32);
      }
#endif
      continue;
    }
    if (decoder.get(byte) == DECODE_OK) {
      ESP_LOGW(TAG, "Unknown key %X", byte);
    }
    break;
  }
}

void PacketTransport::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Packet Transport:\n"
                "  Platform: %s\n"
                "  Encrypted: %s\n"
                "  Ping-pong: %s",
                this->platform_name_, YESNO(this->is_encrypted_()), YESNO(this->ping_pong_enable_));
#ifdef USE_SENSOR
  for (auto sensor : this->sensors_)
    ESP_LOGCONFIG(TAG, "  Sensor: %s", sensor.id);
#endif
#ifdef USE_BINARY_SENSOR
  for (auto sensor : this->binary_sensors_)
    ESP_LOGCONFIG(TAG, "  Binary Sensor: %s", sensor.id);
#endif
  for (uint8_t i = 0; i < this->providers_.count; i++) {
    if (!this->providers_.data[i].active)
      continue;
    ESP_LOGCONFIG(TAG, "  Remote host: %s", this->providers_.data[i].name);
    ESP_LOGCONFIG(TAG, "    Encrypted: %s", YESNO(this->providers_.data[i].key_length > 0));
#ifdef USE_SENSOR
    for (uint8_t j = 0; j < this->remote_sensor_count_; j++) {
      if (this->remote_sensors_[j].active && this->remote_sensors_[j].provider_index == i) {
        ESP_LOGCONFIG(TAG, "    Sensor: %s", this->remote_sensors_[j].sensor_id);
      }
    }
#endif
#ifdef USE_BINARY_SENSOR
    for (uint8_t j = 0; j < this->remote_binary_sensor_count_; j++) {
      if (this->remote_binary_sensors_[j].active && this->remote_binary_sensors_[j].provider_index == i) {
        ESP_LOGCONFIG(TAG, "    Binary Sensor: %s", this->remote_binary_sensors_[j].sensor_id);
      }
    }
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
  this->ping_header_len_ = 0;
  add(this->ping_header_, this->ping_header_len_, MAGIC_PING);
  add(this->ping_header_, this->ping_header_len_, this->name_);
  add(this->ping_header_, this->ping_header_len_, this->ping_key_);
  std::vector<uint8_t> ping_vec(this->ping_header_, this->ping_header_ + this->ping_header_len_);
  this->send_packet(ping_vec);
  this->resend_ping_key_ = false;
  ESP_LOGV(TAG, "Sent new ping request %08X", (unsigned) this->ping_key_);
}

int8_t PacketTransport::find_provider_(const char *name) {
  for (uint8_t i = 0; i < this->providers_.count; i++) {
    if (this->providers_.data[i].active && strcmp(this->providers_.data[i].name, name) == 0) {
      return i;
    }
  }
  return -1;
}

int8_t PacketTransport::find_or_create_provider_(const char *name) {
  int8_t index = this->find_provider_(name);
  if (index >= 0)
    return index;

  if (this->providers_.count >= MAX_PROVIDERS) {
    ESP_LOGE(TAG, "Maximum number of providers (%d) reached", MAX_PROVIDERS);
    return -1;
  }

  index = this->providers_.count++;
  this->providers_.data[index].name = name;
  this->providers_.data[index].key_length = 0;
  this->providers_.data[index].last_code[0] = 0;
  this->providers_.data[index].last_code[1] = 0;
  this->providers_.data[index].last_key_response_time = 0;
  this->providers_.data[index].active = true;
#ifdef USE_STATUS_SENSOR
  this->providers_.data[index].status_sensor = nullptr;
#endif
  return index;
}

void PacketTransport::add_provider(const char *hostname) { this->find_or_create_provider_(hostname); }

void PacketTransport::set_encryption_key(const uint8_t *key, uint8_t key_length) {
  if (key_length > MAX_ENCRYPTION_KEY_SIZE) {
    ESP_LOGE(TAG, "Encryption key too large: %d > %d", key_length, MAX_ENCRYPTION_KEY_SIZE);
    return;
  }
  memcpy(this->encryption_key_, key, key_length);
  this->encryption_key_length_ = key_length;
}

void PacketTransport::set_provider_encryption(const char *name, const uint8_t *key, uint8_t key_length) {
  int8_t index = this->find_or_create_provider_(name);
  if (index < 0)
    return;
  if (key_length > MAX_ENCRYPTION_KEY_SIZE) {
    ESP_LOGE(TAG, "Encryption key too large for provider %s: %d > %d", name, key_length, MAX_ENCRYPTION_KEY_SIZE);
    return;
  }
  memcpy(this->providers_.data[index].encryption_key, key, key_length);
  this->providers_.data[index].key_length = key_length;
}

#ifdef USE_STATUS_SENSOR
void PacketTransport::set_provider_status_sensor(const char *name, binary_sensor::BinarySensor *sensor) {
  int8_t index = this->find_or_create_provider_(name);
  if (index < 0)
    return;
  this->providers_.data[index].status_sensor = sensor;
}
#endif

#ifdef USE_SENSOR
int8_t PacketTransport::find_remote_sensor_(uint8_t provider_index, const char *sensor_id) {
  for (uint8_t i = 0; i < this->remote_sensor_count_; i++) {
    if (this->remote_sensors_[i].active && this->remote_sensors_[i].provider_index == provider_index &&
        strcmp(this->remote_sensors_[i].sensor_id, sensor_id) == 0) {
      return i;
    }
  }
  return -1;
}

void PacketTransport::add_remote_sensor(const char *hostname, const char *remote_id, sensor::Sensor *sensor) {
  int8_t provider_index = this->find_or_create_provider_(hostname);
  if (provider_index < 0)
    return;

  if (this->remote_sensor_count_ >= MAX_REMOTE_SENSORS) {
    ESP_LOGE(TAG, "Maximum number of remote sensors (%d) reached", MAX_REMOTE_SENSORS);
    return;
  }

  this->remote_sensors_[this->remote_sensor_count_].sensor = sensor;
  this->remote_sensors_[this->remote_sensor_count_].sensor_id = remote_id;
  this->remote_sensors_[this->remote_sensor_count_].provider_index = provider_index;
  this->remote_sensors_[this->remote_sensor_count_].active = true;
  this->remote_sensor_count_++;
}
#endif

#ifdef USE_BINARY_SENSOR
int8_t PacketTransport::find_remote_binary_sensor_(uint8_t provider_index, const char *sensor_id) {
  for (uint8_t i = 0; i < this->remote_binary_sensor_count_; i++) {
    if (this->remote_binary_sensors_[i].active && this->remote_binary_sensors_[i].provider_index == provider_index &&
        strcmp(this->remote_binary_sensors_[i].sensor_id, sensor_id) == 0) {
      return i;
    }
  }
  return -1;
}

void PacketTransport::add_remote_binary_sensor(const char *hostname, const char *remote_id,
                                               binary_sensor::BinarySensor *sensor) {
  int8_t provider_index = this->find_or_create_provider_(hostname);
  if (provider_index < 0)
    return;

  if (this->remote_binary_sensor_count_ >= MAX_REMOTE_BINARY_SENSORS) {
    ESP_LOGE(TAG, "Maximum number of remote binary sensors (%d) reached", MAX_REMOTE_BINARY_SENSORS);
    return;
  }

  this->remote_binary_sensors_[this->remote_binary_sensor_count_].sensor = sensor;
  this->remote_binary_sensors_[this->remote_binary_sensor_count_].sensor_id = remote_id;
  this->remote_binary_sensors_[this->remote_binary_sensor_count_].provider_index = provider_index;
  this->remote_binary_sensors_[this->remote_binary_sensor_count_].active = true;
  this->remote_binary_sensor_count_++;
}
#endif

}  // namespace packet_transport
}  // namespace esphome
