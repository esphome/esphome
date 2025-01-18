#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/network/util.h"
#include "udp_component.h"

namespace esphome {
namespace udp {

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
static const char *const TAG = "udp";

/**
 * XXTEA implementation, using 256 bit key.
 */

static const uint32_t DELTA = 0x9e3779b9;
#define MX ((((z >> 5) ^ (y << 2)) + ((y >> 3) ^ (z << 4))) ^ ((sum ^ y) + (k[(p ^ e) & 7] ^ z)))

/**
 * Encrypt a block of data in-place
 */

static void xxtea_encrypt(uint32_t *v, size_t n, const uint32_t *k) {
  uint32_t z, y, sum, e;
  size_t p;
  size_t q = 6 + 52 / n;
  sum = 0;
  z = v[n - 1];
  while (q-- != 0) {
    sum += DELTA;
    e = (sum >> 2);
    for (p = 0; p != n - 1; p++) {
      y = v[p + 1];
      z = v[p] += MX;
    }
    y = v[0];
    z = v[n - 1] += MX;
  }
}

static void xxtea_decrypt(uint32_t *v, size_t n, const uint32_t *k) {
  uint32_t z, y, sum, e;
  size_t p;
  size_t q = 6 + 52 / n;
  sum = q * DELTA;
  y = v[0];
  while (q-- != 0) {
    e = (sum >> 2);
    for (p = n - 1; p != 0; p--) {
      z = v[p - 1];
      y = v[p] -= MX;
    }
    z = v[n - 1];
    y = v[0] -= MX;
    sum -= DELTA;
  }
}

inline static size_t round4(size_t value) { return (value + 3) & ~3; }

union FuData {
  uint32_t u32;
  float f32;
};

static const size_t MAX_PACKET_SIZE = 508;
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

static const size_t MAX_PING_KEYS = 4;

static inline void add(std::vector<uint8_t> &vec, uint32_t data) {
  vec.push_back(data & 0xFF);
  vec.push_back((data >> 8) & 0xFF);
  vec.push_back((data >> 16) & 0xFF);
  vec.push_back((data >> 24) & 0xFF);
}

static inline uint32_t get_uint32(uint8_t *&buf) {
  uint32_t data = *buf++;
  data += *buf++ << 8;
  data += *buf++ << 16;
  data += *buf++ << 24;
  return data;
}

static inline uint16_t get_uint16(uint8_t *&buf) {
  uint16_t data = *buf++;
  data += *buf++ << 8;
  return data;
}

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

void UDPComponent::setup() {
  this->name_ = App.get_name().c_str();
  if (strlen(this->name_) > 255) {
    this->mark_failed();
    this->status_set_error("Device name exceeds 255 chars");
    return;
  }
  this->resend_ping_key_ = this->ping_pong_enable_;
  // restore the upper 32 bits of the rolling code, increment and save.
  this->pref_ = global_preferences->make_preference<uint32_t>(PREF_HASH, true);
  this->pref_.load(&this->rolling_code_[1]);
  this->rolling_code_[1]++;
  this->pref_.save(&this->rolling_code_[1]);
  this->ping_key_ = random_uint32();
  ESP_LOGV(TAG, "Rolling code incremented, upper part now %u", (unsigned) this->rolling_code_[1]);
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
  this->should_send_ = this->ping_pong_enable_;
#ifdef USE_SENSOR
  this->should_send_ |= !this->sensors_.empty();
#endif
#ifdef USE_BINARY_SENSOR
  this->should_send_ |= !this->binary_sensors_.empty();
#endif
  this->should_listen_ = !this->providers_.empty() || this->is_encrypted_();
  // initialise the header. This is invariant.
  add(this->header_, MAGIC_NUMBER);
  add(this->header_, this->name_);
  // pad to a multiple of 4 bytes
  while (this->header_.size() & 0x3)
    this->header_.push_back(0);
#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
  for (const auto &address : this->addresses_) {
    struct sockaddr saddr {};
    socket::set_sockaddr(&saddr, sizeof(saddr), address, this->port_);
    this->sockaddrs_.push_back(saddr);
  }
  // set up broadcast socket
  if (this->should_send_) {
    this->broadcast_socket_ = socket::socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (this->broadcast_socket_ == nullptr) {
      this->mark_failed();
      this->status_set_error("Could not create socket");
      return;
    }
    int enable = 1;
    auto err = this->broadcast_socket_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    if (err != 0) {
      this->status_set_warning("Socket unable to set reuseaddr");
      // we can still continue
    }
    err = this->broadcast_socket_->setsockopt(SOL_SOCKET, SO_BROADCAST, &enable, sizeof(int));
    if (err != 0) {
      this->status_set_warning("Socket unable to set broadcast");
    }
  }
  // create listening socket if we either want to subscribe to providers, or need to listen
  // for ping key broadcasts.
  if (this->should_listen_) {
    this->listen_socket_ = socket::socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (this->listen_socket_ == nullptr) {
      this->mark_failed();
      this->status_set_error("Could not create socket");
      return;
    }
    auto err = this->listen_socket_->setblocking(false);
    if (err < 0) {
      ESP_LOGE(TAG, "Unable to set nonblocking: errno %d", errno);
      this->mark_failed();
      this->status_set_error("Unable to set nonblocking");
      return;
    }
    int enable = 1;
    err = this->listen_socket_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
    if (err != 0) {
      this->status_set_warning("Socket unable to set reuseaddr");
      // we can still continue
    }
    struct sockaddr_in server {};

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = ESPHOME_INADDR_ANY;
    server.sin_port = htons(this->port_);

    err = this->listen_socket_->bind((struct sockaddr *) &server, sizeof(server));
    if (err != 0) {
      ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
      this->mark_failed();
      this->status_set_error("Unable to bind socket");
      return;
    }
  }
#endif
#ifdef USE_SOCKET_IMPL_LWIP_TCP
  // 8266 and RP2040 `Duino
  for (const auto &address : this->addresses_) {
    auto ipaddr = IPAddress();
    ipaddr.fromString(address.c_str());
    this->ipaddrs_.push_back(ipaddr);
  }
  if (this->should_listen_)
    this->udp_client_.begin(this->port_);
#endif
}

void UDPComponent::init_data_() {
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

void UDPComponent::flush_() {
  if (!network::is_connected() || this->data_.empty())
    return;
  uint32_t buffer[MAX_PACKET_SIZE / 4];
  memset(buffer, 0, sizeof buffer);
  // len must be a multiple of 4
  auto header_len = round4(this->header_.size()) / 4;
  auto len = round4(data_.size()) / 4;
  memcpy(buffer, this->header_.data(), this->header_.size());
  memcpy(buffer + header_len, this->data_.data(), this->data_.size());
  if (this->is_encrypted_()) {
    xxtea_encrypt(buffer + header_len, len, (uint32_t *) this->encryption_key_.data());
  }
  auto total_len = (header_len + len) * 4;
  this->send_packet_(buffer, total_len);
}

void UDPComponent::add_binary_data_(uint8_t key, const char *id, bool data) {
  auto len = 1 + 1 + 1 + strlen(id);
  if (len + this->header_.size() + this->data_.size() > MAX_PACKET_SIZE) {
    this->flush_();
  }
  add(this->data_, key);
  add(this->data_, (uint8_t) data);
  add(this->data_, id);
}
void UDPComponent::add_data_(uint8_t key, const char *id, float data) {
  FuData udata{.f32 = data};
  this->add_data_(key, id, udata.u32);
}

void UDPComponent::add_data_(uint8_t key, const char *id, uint32_t data) {
  auto len = 4 + 1 + 1 + strlen(id);
  if (len + this->header_.size() + this->data_.size() > MAX_PACKET_SIZE) {
    this->flush_();
  }
  add(this->data_, key);
  add(this->data_, data);
  add(this->data_, id);
}
void UDPComponent::send_data_(bool all) {
  if (!this->should_send_ || !network::is_connected())
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
  this->resend_data_ = false;
}

void UDPComponent::update() {
  this->updated_ = true;
  this->resend_data_ = this->should_send_;
  auto now = millis() / 1000;
  if (this->last_key_time_ + this->ping_pong_recyle_time_ < now) {
    this->resend_ping_key_ = this->ping_pong_enable_;
    this->last_key_time_ = now;
  }
}

void UDPComponent::loop() {
  uint8_t buf[MAX_PACKET_SIZE];
  if (this->should_listen_) {
    for (;;) {
#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
      auto len = this->listen_socket_->read(buf, sizeof(buf));
#endif
#ifdef USE_SOCKET_IMPL_LWIP_TCP
      auto len = this->udp_client_.parsePacket();
      if (len > 0)
        len = this->udp_client_.read(buf, sizeof(buf));
#endif
      if (len > 0) {
        this->process_(buf, len);
        continue;
      }
      break;
    }
  }
  if (this->resend_ping_key_)
    this->send_ping_pong_request_();
  if (this->updated_) {
    this->send_data_(this->resend_data_);
  }
}

void UDPComponent::add_key_(const char *name, uint32_t key) {
  if (!this->is_encrypted_())
    return;
  if (this->ping_keys_.count(name) == 0 && this->ping_keys_.size() == MAX_PING_KEYS) {
    ESP_LOGW(TAG, "Ping key from %s discarded", name);
    return;
  }
  this->ping_keys_[name] = key;
  this->resend_data_ = true;
  ESP_LOGV(TAG, "Ping key from %s now %X", name, (unsigned) key);
}

void UDPComponent::process_ping_request_(const char *name, uint8_t *ptr, size_t len) {
  if (len != 4) {
    ESP_LOGW(TAG, "Bad ping request");
    return;
  }
  auto key = get_uint32(ptr);
  this->add_key_(name, key);
  ESP_LOGV(TAG, "Updated ping key for %s to %08X", name, (unsigned) key);
}

static bool process_rolling_code(Provider &provider, uint8_t *&buf, const uint8_t *end) {
  if (end - buf < 8)
    return false;
  auto code0 = get_uint32(buf);
  auto code1 = get_uint32(buf);
  if (code1 < provider.last_code[1] || (code1 == provider.last_code[1] && code0 <= provider.last_code[0])) {
    ESP_LOGW(TAG, "Rolling code for %s %08lX:%08lX is old", provider.name, (unsigned long) code1,
             (unsigned long) code0);
    return false;
  }
  provider.last_code[0] = code0;
  provider.last_code[1] = code1;
  return true;
}

/**
 * Process a received packet
 */
void UDPComponent::process_(uint8_t *buf, const size_t len) {
  auto ping_key_seen = !this->ping_pong_enable_;
  if (len < 8) {
    ESP_LOGV(TAG, "Bad length %zu", len);
    return;
  }
  char namebuf[256]{};
  uint8_t byte;
  uint8_t *start_ptr = buf;
  const uint8_t *end = buf + len;
  FuData rdata{};
  auto magic = get_uint16(buf);
  if (magic != MAGIC_NUMBER && magic != MAGIC_PING) {
    ESP_LOGV(TAG, "Bad magic %X", magic);
    return;
  }

  auto hlen = *buf++;
  if (hlen > len - 3) {
    ESP_LOGV(TAG, "Bad hostname length %u > %zu", hlen, len - 3);
    return;
  }
  memcpy(namebuf, buf, hlen);
  if (strcmp(this->name_, namebuf) == 0) {
    ESP_LOGV(TAG, "Ignoring our own data");
    return;
  }
  buf += hlen;
  if (magic == MAGIC_PING) {
    this->process_ping_request_(namebuf, buf, end - buf);
    return;
  }
  if (round4(len) != len) {
    ESP_LOGW(TAG, "Bad length %zu", len);
    return;
  }
  hlen = round4(hlen + 3);
  buf = start_ptr + hlen;
  if (buf == end) {
    ESP_LOGV(TAG, "No data after header");
    return;
  }

  if (this->providers_.count(namebuf) == 0) {
    ESP_LOGVV(TAG, "Unknown hostname %s", namebuf);
    return;
  }
  auto &provider = this->providers_[namebuf];
  // if encryption not used with this host, ping check is pointless since it would be easily spoofed.
  if (provider.encryption_key.empty())
    ping_key_seen = true;

  ESP_LOGV(TAG, "Found hostname %s", namebuf);
#ifdef USE_SENSOR
  auto &sensors = this->remote_sensors_[namebuf];
#endif
#ifdef USE_BINARY_SENSOR
  auto &binary_sensors = this->remote_binary_sensors_[namebuf];
#endif

  if (!provider.encryption_key.empty()) {
    xxtea_decrypt((uint32_t *) buf, (end - buf) / 4, (uint32_t *) provider.encryption_key.data());
  }
  byte = *buf++;
  if (byte == ROLLING_CODE_KEY) {
    if (!process_rolling_code(provider, buf, end))
      return;
  } else if (byte != DATA_KEY) {
    ESP_LOGV(TAG, "Expected rolling_key or data_key, got %X", byte);
    return;
  }
  while (buf < end) {
    byte = *buf++;
    if (byte == ZERO_FILL_KEY)
      continue;
    if (byte == PING_KEY) {
      if (end - buf < 4) {
        ESP_LOGV(TAG, "PING_KEY requires 4 more bytes");
        return;
      }
      auto key = get_uint32(buf);
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
    if (byte == BINARY_SENSOR_KEY) {
      if (end - buf < 3) {
        ESP_LOGV(TAG, "Binary sensor key requires at least 3 more bytes");
        return;
      }
      rdata.u32 = *buf++;
    } else if (byte == SENSOR_KEY) {
      if (end - buf < 6) {
        ESP_LOGV(TAG, "Sensor key requires at least 6 more bytes");
        return;
      }
      rdata.u32 = get_uint32(buf);
    } else {
      ESP_LOGW(TAG, "Unknown key byte %X", byte);
      return;
    }

    hlen = *buf++;
    if (end - buf < hlen) {
      ESP_LOGV(TAG, "Name length of %u not available", hlen);
      return;
    }
    memset(namebuf, 0, sizeof namebuf);
    memcpy(namebuf, buf, hlen);
    ESP_LOGV(TAG, "Found sensor key %d, id %s, data %lX", byte, namebuf, (unsigned long) rdata.u32);
    buf += hlen;
#ifdef USE_SENSOR
    if (byte == SENSOR_KEY && sensors.count(namebuf) != 0)
      sensors[namebuf]->publish_state(rdata.f32);
#endif
#ifdef USE_BINARY_SENSOR
    if (byte == BINARY_SENSOR_KEY && binary_sensors.count(namebuf) != 0)
      binary_sensors[namebuf]->publish_state(rdata.u32 != 0);
#endif
  }
}

void UDPComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "UDP:");
  ESP_LOGCONFIG(TAG, "  Port: %u", this->port_);
  ESP_LOGCONFIG(TAG, "  Encrypted: %s", YESNO(this->is_encrypted_()));
  ESP_LOGCONFIG(TAG, "  Ping-pong: %s", YESNO(this->ping_pong_enable_));
  for (const auto &address : this->addresses_)
    ESP_LOGCONFIG(TAG, "  Address: %s", address.c_str());
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
void UDPComponent::increment_code_() {
  if (this->rolling_code_enable_) {
    if (++this->rolling_code_[0] == 0) {
      this->rolling_code_[1]++;
      this->pref_.save(&this->rolling_code_[1]);
    }
  }
}
void UDPComponent::send_packet_(void *data, size_t len) {
#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
  for (const auto &saddr : this->sockaddrs_) {
    auto result = this->broadcast_socket_->sendto(data, len, 0, &saddr, sizeof(saddr));
    if (result < 0)
      ESP_LOGW(TAG, "sendto() error %d", errno);
  }
#endif
#ifdef USE_SOCKET_IMPL_LWIP_TCP
  auto iface = IPAddress(0, 0, 0, 0);
  for (const auto &saddr : this->ipaddrs_) {
    if (this->udp_client_.beginPacketMulticast(saddr, this->port_, iface, 128) != 0) {
      this->udp_client_.write((const uint8_t *) data, len);
      auto result = this->udp_client_.endPacket();
      if (result == 0)
        ESP_LOGW(TAG, "udp.write() error");
    }
  }
#endif
}

void UDPComponent::send_ping_pong_request_() {
  if (!this->ping_pong_enable_ || !network::is_connected())
    return;
  this->ping_key_ = random_uint32();
  this->ping_header_.clear();
  add(this->ping_header_, MAGIC_PING);
  add(this->ping_header_, this->name_);
  add(this->ping_header_, this->ping_key_);
  this->send_packet_(this->ping_header_.data(), this->ping_header_.size());
  this->resend_ping_key_ = false;
  ESP_LOGV(TAG, "Sent new ping request %08X", (unsigned) this->ping_key_);
}
}  // namespace udp
}  // namespace esphome
