#include "entity_transport.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::entity_transport {

using packet_interface::PacketBuffer;
using packet_interface::PacketMetaData;

ESPHOME_LOG_TAG(TAG, "entity_transport");

// The packet type byte holds the format version in its high nibble
static constexpr uint8_t PACKET_DICTIONARY = 0x11;
static constexpr uint8_t PACKET_DATA = 0x12;
static constexpr uint8_t PACKET_REQUEST = 0x13;
static constexpr uint8_t TYPE_SENSOR = 1;
static constexpr uint8_t TYPE_BINARY_SENSOR = 2;
static constexpr uint8_t BINARY_SENSOR_NO_STATE = 2;
// Packet type and dictionary id
static constexpr size_t HEADER_SIZE = 5;
// The data packet header adds the number of sensor records
static constexpr size_t SENSOR_COUNT_OFFSET = HEADER_SIZE;
static constexpr size_t DATA_HEADER_SIZE = HEADER_SIZE + 1;
static constexpr size_t SENSOR_RECORD_SIZE = 5;
static constexpr size_t BINARY_SENSOR_RECORD_SIZE = 2;
// A dictionary section starts with the entity type, first number and count
static constexpr size_t SECTION_HEADER_SIZE = 3;
static constexpr size_t MAX_STRING_LENGTH = 255;
static constexpr uint32_t DEFAULT_UPDATE_INTERVAL_MS = 15000;
static constexpr uint32_t REQUEST_INTERVAL_MS = 5000;
static constexpr uint32_t RETRY_DELAY_MS = 1000;
static constexpr uint32_t RETRY_TIMER_ID = 0;
static constexpr uint32_t TIMEOUT_CHECK_INTERVAL_MS = 1000;

static void put_uint32(uint8_t *buf, uint32_t value) {
  for (size_t i = 0; i != 4; i++)
    buf[i] = value >> (i * 8);
}

// True if the NUL terminated str is the same as the len bytes at data
static bool matches(const char *str, const char *data, size_t len) {
  return strlen(str) == len && memcmp(str, data, len) == 0;
}

void EntityTransport::setup() {
  this->interface_ = this->interface_->top();
  this->max_packet_size_ = this->interface_->get_max_packet_size();
  const auto &name = App.get_name();
  size_t longest_id = 0;
  for (const auto &entity : this->entities_)
    longest_id = std::max(longest_id, strlen(entity.id));
  // A dictionary packet must hold the device name and any one id, which also leaves room for a data record
  if (name.size() > MAX_STRING_LENGTH || longest_id > MAX_STRING_LENGTH ||
      HEADER_SIZE + 1 + name.size() + SECTION_HEADER_SIZE + 1 + longest_id > this->max_packet_size_) {
    this->mark_failed(LOG_STR("max_packet_size too small for the device name and entity ids"));
    return;
  }
  RAMAllocator<uint8_t> allocator;
  this->buffer_ = allocator.allocate(this->max_packet_size_);
  if (this->buffer_ == nullptr) {
    this->mark_failed(LOG_STR("Not enough memory for the packet buffer"));
    return;
  }
  // The dictionary id changes when the device name or any entity changes
  uint32_t hash = fnv1a_hash(name.c_str());
  for (const auto &entity : this->entities_) {
    hash = fnv1a_hash_extend(hash, entity.is_sensor ? TYPE_SENSOR : TYPE_BINARY_SENSOR);
    hash = fnv1a_hash_extend(hash, entity.id);
  }
  this->dictionary_id_ = hash;

  // [&entity] is safe: FixedVector never reallocates, and the parent pointer keeps the capture to one pointer
  for (auto &entity : this->entities_) {
#ifdef USE_SENSOR
    if (entity.is_sensor) {
      static_cast<sensor::Sensor *>(entity.entity)->add_on_state_callback([&entity](float) {
        entity.parent->mark_updated_(entity);
      });
    }
#endif
#ifdef USE_BINARY_SENSOR
    if (!entity.is_sensor) {
      // The full state callback also fires for the first state
      static_cast<binary_sensor::BinarySensor *>(entity.entity)
          ->add_full_state_callback(
              [&entity](optional<bool>, optional<bool>) { entity.parent->mark_updated_(entity); });
    }
#endif
  }
  this->interface_->add_packet_interface_listener(
      [this](const PacketBuffer &data, const PacketMetaData &) { this->receive_(data); });

  bool session = this->interface_->has_capability(packet_interface::CAPABILITY_SESSION);
  if (session)
    this->interface_->add_on_session_state_callback([this](bool connected) { this->on_session_(connected); });
  // Without a session and reliable delivery, a lost dictionary or state is only replaced by a periodic resend
  if (this->update_interval_ms_ == 0) {
    this->update_interval_ms_ = session && this->interface_->has_capability(packet_interface::CAPABILITY_RELIABILITY)
                                    ? SCHEDULER_DONT_RUN
                                    : DEFAULT_UPDATE_INTERVAL_MS;
  }
  if (this->update_interval_ms_ != SCHEDULER_DONT_RUN)
    this->set_interval(this->update_interval_ms_, [this] { this->send_all_(); });
  if (this->remote_timeout_ms_ != 0)
    this->set_interval(TIMEOUT_CHECK_INTERVAL_MS, [this] { this->clear_remote_states_(true); });
  this->send_all_();
}

void EntityTransport::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Entity Transport:\n"
                "  Packet Interface: %s\n"
                "  Max Packet Size: %zu\n"
                "  Encryption: %s\n"
                "  Reliability: %s\n"
                "  Session: %s\n"
                "  Dictionary ID: %08" PRIX32,
                this->interface_->get_name(), this->max_packet_size_,
                YESNO(this->interface_->has_capability(packet_interface::CAPABILITY_ENCRYPTION)),
                YESNO(this->interface_->has_capability(packet_interface::CAPABILITY_RELIABILITY)),
                YESNO(this->interface_->has_capability(packet_interface::CAPABILITY_SESSION)), this->dictionary_id_);
  if (this->update_interval_ms_ == SCHEDULER_DONT_RUN) {
    ESP_LOGCONFIG(TAG, "  Update Interval: never");
  } else {
    ESP_LOGCONFIG(TAG, "  Update Interval: %" PRIu32 "ms", this->update_interval_ms_);
  }
  if (this->remote_timeout_ms_ != 0) {
    ESP_LOGCONFIG(TAG, "  Remote Timeout: %" PRIu32 "ms", this->remote_timeout_ms_);
  }
  for (const auto &entity : this->entities_) {
    ESP_LOGCONFIG(TAG, "  %s %u: %s", entity.is_sensor ? LOG_STR_LITERAL("Sensor") : LOG_STR_LITERAL("Binary Sensor"),
                  entity.number, entity.id);
  }
#ifdef ENTITY_TRANSPORT_REMOTE_SENSOR_COUNT
  for (const auto &remote : this->remote_sensors_) {
    ESP_LOGCONFIG(TAG, "  Remote Sensor: %s from %s", remote.remote_id, remote.provider);
  }
#endif
#ifdef ENTITY_TRANSPORT_REMOTE_BINARY_SENSOR_COUNT
  for (const auto &remote : this->remote_binary_sensors_) {
    ESP_LOGCONFIG(TAG, "  Remote Binary Sensor: %s from %s", remote.remote_id, remote.provider);
  }
#endif
}

void EntityTransport::send_all_() {
  this->dictionary_pending_ = true;
  this->dictionary_next_ = 0;
  for (auto &entity : this->entities_)
    entity.updated = true;
  this->enable_loop();
}

void EntityTransport::mark_updated_(LocalEntity &entity) {
  entity.updated = true;
  this->enable_loop();
}

void EntityTransport::loop() {
  this->disable_loop();
  // States are only sent once the dictionary that names them has been accepted
  if ((this->dictionary_pending_ && !this->send_dictionary_()) || !this->send_updated_())
    this->set_timeout(RETRY_TIMER_ID, RETRY_DELAY_MS, [this] { this->enable_loop(); });
}

size_t EntityTransport::write_header_(uint8_t type) const {
  this->buffer_[0] = type;
  put_uint32(this->buffer_ + 1, this->dictionary_id_);
  return HEADER_SIZE;
}

bool EntityTransport::send_dictionary_() {
  uint8_t *buf = this->buffer_;
  const auto &name = App.get_name();
  size_t header = this->write_header_(PACKET_DICTIONARY);
  buf[header++] = name.size();
  memcpy(buf + header, name.c_str(), name.size());
  header += name.size();
  size_t length = header;
  size_t count_offset = 0;
  uint8_t section_type = 0;  // none
  // A dictionary in several packets resumes after the last packet accepted, so a refused one does not resend the rest
  for (size_t i = this->dictionary_next_; i != this->entities_.size(); i++) {
    const auto &entity = this->entities_[i];
    uint8_t type = entity.is_sensor ? TYPE_SENSOR : TYPE_BINARY_SENSOR;
    size_t id_length = strlen(entity.id);
    bool new_section = type != section_type;
    if (length + (new_section ? SECTION_HEADER_SIZE : 0) + 1 + id_length > this->max_packet_size_) {
      if (!this->interface_->send_to_interface(PacketBuffer(buf, length), {}))
        return false;
      this->dictionary_next_ = i;
      length = header;
      new_section = true;
    }
    if (new_section) {
      buf[length++] = type;
      buf[length++] = entity.number;
      count_offset = length;
      buf[length++] = 0;
      section_type = type;
    }
    buf[count_offset]++;
    buf[length++] = id_length;
    memcpy(buf + length, entity.id, id_length);
    length += id_length;
  }
  if (length != header && !this->interface_->send_to_interface(PacketBuffer(buf, length), {}))
    return false;
  this->dictionary_pending_ = false;
  this->dictionary_next_ = 0;
  return true;
}

// Sends every updated entity, in as few packets as fit; returns false if a packet was refused
bool EntityTransport::send_updated_() {
  this->write_header_(PACKET_DATA);
  this->buffer_[SENSOR_COUNT_OFFSET] = 0;
  size_t length = DATA_HEADER_SIZE;
  size_t first = 0;
  for (size_t i = 0; i != this->entities_.size(); i++) {
    auto &entity = this->entities_[i];
    if (!entity.updated)
      continue;
    size_t record_size = entity.is_sensor ? SENSOR_RECORD_SIZE : BINARY_SENSOR_RECORD_SIZE;
    if (length + record_size > this->max_packet_size_) {
      if (!this->flush_(length, first, i))
        return false;
      length = DATA_HEADER_SIZE;
      this->buffer_[SENSOR_COUNT_OFFSET] = 0;
      first = i;
    }
    uint8_t *record = this->buffer_ + length;
    record[0] = entity.number;
#ifdef USE_SENSOR
    if (entity.is_sensor) {
      float state = static_cast<sensor::Sensor *>(entity.entity)->get_state();
      uint32_t bits;
      memcpy(&bits, &state, sizeof(bits));
      put_uint32(record + 1, bits);
      this->buffer_[SENSOR_COUNT_OFFSET]++;
    }
#endif
#ifdef USE_BINARY_SENSOR
    if (!entity.is_sensor) {
      auto *sensor = static_cast<binary_sensor::BinarySensor *>(entity.entity);
      record[1] = sensor->has_state() ? sensor->state : BINARY_SENSOR_NO_STATE;
    }
#endif
    length += record_size;
  }
  return this->flush_(length, first, this->entities_.size());
}

// Sends the packet holding the updated entities from first up to last, and clears their flags if it was accepted
bool EntityTransport::flush_(size_t length, size_t first, size_t last) {
  if (length == DATA_HEADER_SIZE)
    return true;
  if (!this->interface_->send_to_interface(PacketBuffer(this->buffer_, length), {})) {
    ESP_LOGV(TAG, "Packet of %zu bytes not accepted, will retry", length);
    return false;
  }
  for (size_t i = first; i != last; i++)
    this->entities_[i].updated = false;
  return true;
}

void EntityTransport::receive_(const PacketBuffer &data) {
  // Packet interface layers deliver packets in one piece
  if (!data.is_contiguous()) {
    ESP_LOGW(TAG, "Packet split across buffers is not supported");
    return;
  }
  auto packet = data.as_span();
  if (packet.size() < HEADER_SIZE)
    return;
  uint32_t dictionary_id = encode_uint32(packet[4], packet[3], packet[2], packet[1]);
  if (dictionary_id == this->dictionary_id_) {
    // Either a request for our dictionary, or our own packet heard on a broadcast channel
    if (packet[0] == PACKET_REQUEST) {
      ESP_LOGD(TAG, "Dictionary requested");
      this->send_all_();
    }
    return;
  }
  if (packet[0] == PACKET_DICTIONARY) {
    this->receive_dictionary_(packet, dictionary_id);
  } else if (packet[0] == PACKET_DATA) {
    this->receive_data_(packet, dictionary_id);
  } else if (packet[0] != PACKET_REQUEST) {
    ESP_LOGV(TAG, "Unknown packet type 0x%02X", packet[0]);
  }
}

void EntityTransport::receive_dictionary_(std::span<const uint8_t> packet, uint32_t dictionary_id) {
  size_t pos = HEADER_SIZE;
  if (pos == packet.size() || pos + 1 + packet[pos] > packet.size())
    return;
  size_t provider_length = packet[pos++];
  const auto *provider = reinterpret_cast<const char *>(packet.data() + pos);
  pos += provider_length;
  while (pos != packet.size()) {
    if (pos + SECTION_HEADER_SIZE > packet.size())
      return;
    uint8_t type = packet[pos];
    uint8_t number = packet[pos + 1];
    uint8_t count = packet[pos + 2];
    pos += SECTION_HEADER_SIZE;
    for (uint8_t i = 0; i != count; i++) {
      if (pos == packet.size() || pos + 1 + packet[pos] > packet.size())
        return;
      size_t id_length = packet[pos++];
      this->learn_(provider, provider_length, type, number + i, reinterpret_cast<const char *>(packet.data() + pos),
                   id_length, dictionary_id);
      pos += id_length;
    }
  }
}

void EntityTransport::learn_(const char *provider, size_t provider_length, uint8_t type, uint8_t number, const char *id,
                             size_t id_length, uint32_t dictionary_id) {
#ifdef ENTITY_TRANSPORT_REMOTE_SENSOR_COUNT
  if (type == TYPE_SENSOR) {
    for (auto &remote : this->remote_sensors_) {
      if (matches(remote.provider, provider, provider_length) && matches(remote.remote_id, id, id_length)) {
        remote.dictionary_id = dictionary_id;
        remote.number = number;
        remote.known = true;
      }
    }
  }
#endif
#ifdef ENTITY_TRANSPORT_REMOTE_BINARY_SENSOR_COUNT
  if (type == TYPE_BINARY_SENSOR) {
    for (auto &remote : this->remote_binary_sensors_) {
      if (matches(remote.provider, provider, provider_length) && matches(remote.remote_id, id, id_length)) {
        remote.dictionary_id = dictionary_id;
        remote.number = number;
        remote.known = true;
      }
    }
  }
#endif
}

void EntityTransport::receive_data_(std::span<const uint8_t> packet, uint32_t dictionary_id) {
  bool known = false;
  bool unresolved = false;
#ifdef ENTITY_TRANSPORT_REMOTE_SENSOR_COUNT
  for (const auto &remote : this->remote_sensors_) {
    known |= remote.known && remote.dictionary_id == dictionary_id;
    unresolved |= !remote.known;
  }
#endif
#ifdef ENTITY_TRANSPORT_REMOTE_BINARY_SENSOR_COUNT
  for (const auto &remote : this->remote_binary_sensors_) {
    known |= remote.known && remote.dictionary_id == dictionary_id;
    unresolved |= !remote.known;
  }
#endif
  if (!known) {
    // The packet may come from the provider of an entity whose number is not yet known
    if (unresolved)
      this->request_dictionary_(dictionary_id);
    return;
  }
  if (packet.size() < DATA_HEADER_SIZE)
    return;
  size_t sensor_end = DATA_HEADER_SIZE + packet[SENSOR_COUNT_OFFSET] * SENSOR_RECORD_SIZE;
  if (sensor_end > packet.size() || (packet.size() - sensor_end) % BINARY_SENSOR_RECORD_SIZE != 0) {
    ESP_LOGV(TAG, "Data packet of %zu bytes is malformed", packet.size());
    return;
  }
  [[maybe_unused]] uint32_t now = App.get_loop_component_start_time();
#ifdef ENTITY_TRANSPORT_REMOTE_SENSOR_COUNT
  for (size_t pos = DATA_HEADER_SIZE; pos != sensor_end; pos += SENSOR_RECORD_SIZE) {
    uint32_t bits = encode_uint32(packet[pos + 4], packet[pos + 3], packet[pos + 2], packet[pos + 1]);
    float value;
    memcpy(&value, &bits, sizeof(value));
    for (auto &remote : this->remote_sensors_) {
      if (remote.known && remote.dictionary_id == dictionary_id && remote.number == packet[pos]) {
        remote.last_state_ms = now;
        remote.has_state = true;
        remote.entity->publish_state(value);
      }
    }
  }
#endif
#ifdef ENTITY_TRANSPORT_REMOTE_BINARY_SENSOR_COUNT
  for (size_t pos = sensor_end; pos != packet.size(); pos += BINARY_SENSOR_RECORD_SIZE) {
    for (auto &remote : this->remote_binary_sensors_) {
      if (!remote.known || remote.dictionary_id != dictionary_id || remote.number != packet[pos])
        continue;
      remote.last_state_ms = now;
      remote.has_state = true;
      if (packet[pos + 1] == BINARY_SENSOR_NO_STATE) {
        remote.entity->invalidate_state();
      } else {
        remote.entity->publish_state(packet[pos + 1] != 0);
      }
    }
  }
#endif
}

void EntityTransport::request_dictionary_(uint32_t dictionary_id) {
  uint32_t now = App.get_loop_component_start_time();
  if (this->request_sent_ && now - this->last_request_ms_ < REQUEST_INTERVAL_MS)
    return;
  this->request_sent_ = true;
  this->last_request_ms_ = now;
  ESP_LOGD(TAG, "Requesting dictionary %08" PRIX32, dictionary_id);
  uint8_t packet[HEADER_SIZE] = {PACKET_REQUEST};
  put_uint32(packet + 1, dictionary_id);
  this->interface_->send_to_interface(PacketBuffer(packet, sizeof(packet)), {});
}

// A newly connected or restarted peer needs the dictionary and every state; while it is lost, its entities have no
// state
void EntityTransport::on_session_(bool connected) {
  if (connected) {
    this->send_all_();
    return;
  }
  this->clear_remote_states_(false);
}

// Clears the state of every remote entity, or with timed_out_only, of those whose state is older than the timeout
void EntityTransport::clear_remote_states_(bool timed_out_only) {
  [[maybe_unused]] uint32_t now = App.get_loop_component_start_time();
#ifdef ENTITY_TRANSPORT_REMOTE_SENSOR_COUNT
  for (auto &remote : this->remote_sensors_) {
    if (timed_out_only && (!remote.has_state || now - remote.last_state_ms <= this->remote_timeout_ms_))
      continue;
    remote.has_state = false;
    remote.entity->publish_state(NAN);
  }
#endif
#ifdef ENTITY_TRANSPORT_REMOTE_BINARY_SENSOR_COUNT
  for (auto &remote : this->remote_binary_sensors_) {
    if (timed_out_only && (!remote.has_state || now - remote.last_state_ms <= this->remote_timeout_ms_))
      continue;
    remote.has_state = false;
    remote.entity->invalidate_state();
  }
#endif
}

}  // namespace esphome::entity_transport
