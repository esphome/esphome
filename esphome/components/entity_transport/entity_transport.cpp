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
static constexpr uint8_t FORMAT_VERSION = 1;
static constexpr uint8_t PACKET_DICTIONARY = 0x11;
static constexpr uint8_t PACKET_DATA = 0x12;
static constexpr uint8_t PACKET_REQUEST = 0x13;
// Entity types, in dictionary sections and data records; 0 is not used
static constexpr uint8_t TYPE_SENSOR = 1;
static constexpr uint8_t TYPE_BINARY_SENSOR = 2;
static constexpr uint8_t MAX_TYPE = 31;
// Packet type and dictionary id
static constexpr size_t HEADER_SIZE = 5;
// A dictionary packet's part byte follows the header: the part's index in bits 0-6, and on the last part, bit 7
static constexpr uint8_t PART_INDEX_MASK = 0x7F;
static constexpr uint8_t PART_LAST = 0x80;
static constexpr uint8_t PART_NONE = 0xFF;  // no dictionary is being received in order
// A data record starts with a 16-bit tag: the entity's number in bits 0-7, its type in bits 8-12, the payload form in
// bits 13-14, and with PAYLOAD_NONE, the state in bit 15. Any record can be skipped without knowing its type.
static constexpr size_t TAG_SIZE = 2;
static constexpr uint8_t TAG_TYPE_SHIFT = 8;
static constexpr uint8_t TAG_PAYLOAD_SHIFT = 13;
static constexpr uint16_t TAG_VALUE_BIT = 0x8000;
static constexpr uint8_t PAYLOAD_NONE = 0;      // the state is in the tag's value bit
static constexpr uint8_t PAYLOAD_WORD = 1;      // 4 bytes follow
static constexpr uint8_t PAYLOAD_LENGTH = 2;    // a length byte follows, then that many bytes
static constexpr uint8_t PAYLOAD_NO_STATE = 3;  // the entity has no state
static constexpr size_t WORD_SIZE = 4;
static constexpr size_t MAX_RECORD_SIZE = TAG_SIZE + WORD_SIZE;
// A dictionary section starts with the entity type, first number and count
static constexpr size_t SECTION_HEADER_SIZE = 3;
static constexpr size_t MAX_STRING_LENGTH = 255;
static constexpr uint32_t DEFAULT_UPDATE_INTERVAL_MS = 15000;
static constexpr uint32_t REQUEST_INTERVAL_MS = 5000;
static constexpr uint32_t RETRY_DELAY_MS = 1000;
static constexpr uint32_t RETRY_TIMER_ID = 0;
static constexpr uint32_t TIMEOUT_CHECK_INTERVAL_MS = 1000;
static constexpr uint32_t WARNING_INTERVAL_MS = 60000;
// How long the interface may refuse every packet before it is reported
static constexpr uint32_t REFUSED_WARNING_MS = 60000;

static void put_uint32(uint8_t *buf, uint32_t value) {
  for (size_t i = 0; i != 4; i++)
    buf[i] = value >> (i * 8);
}

// True if the NUL terminated str is the same as the len bytes at data
static bool matches(const char *str, const char *data, size_t len) {
  return strlen(str) == len && memcmp(str, data, len) == 0;
}

struct Record {
  uint8_t number;
  uint8_t type;
  uint8_t payload;
  bool value;
  const uint8_t *data;  // what follows the tag
};

static Record parse_record(std::span<const uint8_t> packet, size_t pos) {
  uint16_t tag = encode_uint16(packet[pos + 1], packet[pos]);
  return {static_cast<uint8_t>(tag), static_cast<uint8_t>((tag >> TAG_TYPE_SHIFT) & MAX_TYPE),
          static_cast<uint8_t>((tag >> TAG_PAYLOAD_SHIFT) & 3), (tag & TAG_VALUE_BIT) != 0,
          packet.data() + pos + TAG_SIZE};
}

// The size of the data record at pos, or 0 if it does not fit in the packet
static size_t record_size(std::span<const uint8_t> packet, size_t pos) {
  if (pos + TAG_SIZE > packet.size())
    return 0;
  size_t size = TAG_SIZE;
  uint8_t payload = parse_record(packet, pos).payload;
  if (payload == PAYLOAD_WORD) {
    size += WORD_SIZE;
  } else if (payload == PAYLOAD_LENGTH) {
    if (pos + TAG_SIZE == packet.size())
      return 0;
    size += 1 + packet[pos + TAG_SIZE];
  }
  return pos + size <= packet.size() ? size : 0;
}

// Checks every record of a data packet, so that a damaged or forged packet changes no state at all
static bool data_well_formed(std::span<const uint8_t> packet) {
  for (size_t pos = HEADER_SIZE; pos != packet.size();) {
    size_t size = record_size(packet, pos);
    if (size == 0)
      return false;
    Record record = parse_record(packet, pos);
    if (record.type == 0 || (record.value && record.payload != PAYLOAD_NONE))
      return false;
    // Types this device knows must use their own payload form; others are skipped
    if (record.type == TYPE_SENSOR && record.payload != PAYLOAD_WORD && record.payload != PAYLOAD_NO_STATE)
      return false;
    if (record.type == TYPE_BINARY_SENSOR && record.payload != PAYLOAD_NONE && record.payload != PAYLOAD_NO_STATE)
      return false;
    pos += size;
  }
  return true;
}

// Checks the device name and every section of a dictionary packet before anything is learnt from it
static bool dictionary_well_formed(std::span<const uint8_t> packet) {
  size_t pos = HEADER_SIZE + 1;
  if (pos >= packet.size())
    return false;
  pos += 1 + packet[pos];
  if (pos > packet.size())
    return false;
  while (pos != packet.size()) {
    if (pos + SECTION_HEADER_SIZE > packet.size())
      return false;
    uint8_t type = packet[pos];
    size_t first = packet[pos + 1];
    size_t count = packet[pos + 2];
    pos += SECTION_HEADER_SIZE;
    // Numbers are one byte, and a data record can name types up to MAX_TYPE only
    if (type == 0 || type > MAX_TYPE || count == 0 || first + count > 256)
      return false;
    for (size_t i = 0; i != count; i++) {
      if (pos == packet.size())
        return false;
      pos += 1 + packet[pos];
      if (pos > packet.size())
        return false;
    }
  }
  return true;
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
      HEADER_SIZE + 2 + name.size() + SECTION_HEADER_SIZE + 1 + longest_id > this->max_packet_size_) {
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
  this->dictionary_part_ = 0;
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
  if ((this->dictionary_pending_ && !this->send_dictionary_()) || !this->send_updated_()) {
    this->sending_refused_();
    this->set_timeout(RETRY_TIMER_ID, RETRY_DELAY_MS, [this] { this->enable_loop(); });
    return;
  }
  this->refusing_ = false;
  if (this->status_has_warning()) {
    ESP_LOGI(TAG, "Packets are accepted again");
    this->status_clear_warning();
  }
}

// A link that is down or full refuses packets for a while; one that refuses them for long sends nothing at all
void EntityTransport::sending_refused_() {
  uint32_t now = App.get_loop_component_start_time();
  if (!this->refusing_) {
    this->refusing_ = true;
    this->refused_since_ms_ = now;
    return;
  }
  if (!this->status_has_warning() && now - this->refused_since_ms_ >= REFUSED_WARNING_MS) {
    ESP_LOGW(TAG, "The packet interface has accepted no packets for %" PRIu32 "s, so no states are sent",
             (now - this->refused_since_ms_) / 1000);
    this->status_set_warning();
  }
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
  size_t part_offset = header++;
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
      buf[part_offset] = this->dictionary_part_ & PART_INDEX_MASK;
      if (!this->interface_->send_to_interface(PacketBuffer(buf, length), {})) {
        ESP_LOGV(TAG, "Dictionary packet of %zu bytes not accepted, will retry", length);
        return false;
      }
      this->dictionary_next_ = i;
      this->dictionary_part_++;
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
  if (length != header) {
    buf[part_offset] = (this->dictionary_part_ & PART_INDEX_MASK) | PART_LAST;
    if (!this->interface_->send_to_interface(PacketBuffer(buf, length), {})) {
      ESP_LOGV(TAG, "Dictionary packet of %zu bytes not accepted, will retry", length);
      return false;
    }
  }
  this->dictionary_pending_ = false;
  this->dictionary_next_ = 0;
  this->dictionary_part_ = 0;
  return true;
}

// Writes the entity's data record, and returns its size
size_t EntityTransport::write_record_(const LocalEntity &entity, uint8_t *record) const {
  uint8_t type = entity.is_sensor ? TYPE_SENSOR : TYPE_BINARY_SENSOR;
  uint8_t payload = PAYLOAD_NO_STATE;
  bool value = false;
  size_t size = TAG_SIZE;
#ifdef USE_SENSOR
  if (entity.is_sensor) {
    float state = static_cast<sensor::Sensor *>(entity.entity)->get_state();
    if (!std::isnan(state)) {
      payload = PAYLOAD_WORD;
      uint32_t bits;
      memcpy(&bits, &state, sizeof(bits));
      put_uint32(record + TAG_SIZE, bits);
      size += WORD_SIZE;
    }
  }
#endif
#ifdef USE_BINARY_SENSOR
  if (!entity.is_sensor) {
    auto *sensor = static_cast<binary_sensor::BinarySensor *>(entity.entity);
    if (sensor->has_state()) {
      payload = PAYLOAD_NONE;
      value = sensor->state;
    }
  }
#endif
  uint16_t tag =
      entity.number | (type << TAG_TYPE_SHIFT) | (payload << TAG_PAYLOAD_SHIFT) | (value ? TAG_VALUE_BIT : 0);
  record[0] = tag;
  record[1] = tag >> 8;
  return size;
}

// Sends every updated entity, in as few packets as fit; returns false if a packet was refused
bool EntityTransport::send_updated_() {
  size_t length = this->write_header_(PACKET_DATA);
  size_t first = 0;
  for (size_t i = 0; i != this->entities_.size(); i++) {
    auto &entity = this->entities_[i];
    if (!entity.updated)
      continue;
    uint8_t record[MAX_RECORD_SIZE];
    size_t size = this->write_record_(entity, record);
    if (length + size > this->max_packet_size_) {
      if (!this->flush_(length, first, i))
        return false;
      length = HEADER_SIZE;
      first = i;
    }
    memcpy(this->buffer_ + length, record, size);
    length += size;
  }
  return this->flush_(length, first, this->entities_.size());
}

// Sends the packet holding the updated entities from first up to last, and clears their flags if it was accepted
bool EntityTransport::flush_(size_t length, size_t first, size_t last) {
  if (length == HEADER_SIZE)
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
  if (packet.size() < HEADER_SIZE) {
    if (this->warning_due_()) {
      ESP_LOGW(TAG, "Packet of %zu bytes is too short", packet.size());
    }
    return;
  }
  if (packet[0] >> 4 != FORMAT_VERSION) {
    if (this->warning_due_()) {
      ESP_LOGW(TAG, "Packet format version %u not supported; the sender may run another ESPHome version",
               packet[0] >> 4);
    }
    return;
  }
  uint32_t dictionary_id = encode_uint32(packet[4], packet[3], packet[2], packet[1]);
  if (dictionary_id == this->dictionary_id_) {
    // Either a request for our dictionary, or our own packet heard on a broadcast channel
    if (packet[0] == PACKET_REQUEST && packet.size() == HEADER_SIZE) {
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
  if (!dictionary_well_formed(packet)) {
    if (this->warning_due_()) {
      ESP_LOGW(TAG, "Dictionary packet of %zu bytes is malformed", packet.size());
    }
    return;
  }
  size_t pos = HEADER_SIZE;
  uint8_t part = packet[pos++];
  uint8_t index = part & PART_INDEX_MASK;
  // The dictionary is complete, and need not be asked for again, once its last part follows all the others in order
  bool in_order = index == 0 || (this->rx_dictionary_id_ == dictionary_id && index == this->rx_next_part_);
  this->rx_dictionary_id_ = dictionary_id;
  this->rx_next_part_ = in_order ? (index + 1) & PART_INDEX_MASK : PART_NONE;
  bool completed = in_order && (part & PART_LAST) != 0;
  bool first_time = completed && !this->dictionary_seen_(dictionary_id);
  if (first_time) {
    this->seen_dictionaries_[this->seen_next_] = dictionary_id;
    this->seen_next_ = (this->seen_next_ + 1) % this->seen_dictionaries_.size();
  }
  size_t provider_length = packet[pos++];
  const auto *provider = reinterpret_cast<const char *>(packet.data() + pos);
  pos += provider_length;
  while (pos != packet.size()) {
    uint8_t type = packet[pos];
    uint8_t number = packet[pos + 1];
    uint8_t count = packet[pos + 2];
    pos += SECTION_HEADER_SIZE;
    for (uint8_t i = 0; i != count; i++) {
      size_t id_length = packet[pos++];
      this->learn_(provider, provider_length, type, number + i, reinterpret_cast<const char *>(packet.data() + pos),
                   id_length, dictionary_id);
      pos += id_length;
    }
  }
  // A provider may go back to a dictionary already seen, so every complete one is checked
  if (completed)
    this->forget_unnamed_(provider, provider_length, dictionary_id, first_time);
}

void EntityTransport::forget_unnamed_(const char *provider, size_t provider_length, uint32_t dictionary_id,
                                      bool first_time) {
  // A remote entity that its provider's whole dictionary does not name would otherwise keep no or a stale state
  // silently
#ifdef ENTITY_TRANSPORT_REMOTE_SENSOR_COUNT
  for (auto &remote : this->remote_sensors_) {
    if (!matches(remote.provider, provider, provider_length) || (remote.known && remote.dictionary_id == dictionary_id))
      continue;
    if (remote.known || first_time) {
      ESP_LOGW(TAG, "'%s' does not send '%s'", remote.provider, remote.remote_id);
    }
    remote.known = false;
    if (remote.has_state) {
      remote.has_state = false;
      remote.entity->publish_state(NAN);
    }
  }
#endif
#ifdef ENTITY_TRANSPORT_REMOTE_BINARY_SENSOR_COUNT
  for (auto &remote : this->remote_binary_sensors_) {
    if (!matches(remote.provider, provider, provider_length) || (remote.known && remote.dictionary_id == dictionary_id))
      continue;
    if (remote.known || first_time) {
      ESP_LOGW(TAG, "'%s' does not send '%s'", remote.provider, remote.remote_id);
    }
    remote.known = false;
    if (remote.has_state) {
      remote.has_state = false;
      remote.entity->invalidate_state();
    }
  }
#endif
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

bool EntityTransport::dictionary_seen_(uint32_t dictionary_id) const {
  return std::find(this->seen_dictionaries_.begin(), this->seen_dictionaries_.end(), dictionary_id) !=
         this->seen_dictionaries_.end();
}

void EntityTransport::receive_data_(std::span<const uint8_t> packet, uint32_t dictionary_id) {
  bool known = false;
  bool has_remotes = false;
#ifdef ENTITY_TRANSPORT_REMOTE_SENSOR_COUNT
  for (const auto &remote : this->remote_sensors_) {
    known |= remote.known && remote.dictionary_id == dictionary_id;
    has_remotes = true;
  }
#endif
#ifdef ENTITY_TRANSPORT_REMOTE_BINARY_SENSOR_COUNT
  for (const auto &remote : this->remote_binary_sensors_) {
    known |= remote.known && remote.dictionary_id == dictionary_id;
    has_remotes = true;
  }
#endif
  if (!has_remotes)
    return;
  // Checked first, so a damaged packet does not make the provider resend everything
  if (!data_well_formed(packet)) {
    if (this->warning_due_()) {
      ESP_LOGW(TAG, "Data packet of %zu bytes is malformed", packet.size());
    }
    return;
  }
  // A dictionary not yet received in full may name our entities, or be a provider's new one; one received in full
  // named all of them it could, so asking again would only make that provider resend everything
  if (!this->dictionary_seen_(dictionary_id))
    this->request_dictionary_(dictionary_id);
  if (!known)
    return;
  [[maybe_unused]] uint32_t now = App.get_loop_component_start_time();
  for (size_t pos = HEADER_SIZE; pos != packet.size(); pos += record_size(packet, pos)) {
    [[maybe_unused]] Record record = parse_record(packet, pos);
#ifdef ENTITY_TRANSPORT_REMOTE_SENSOR_COUNT
    if (record.type == TYPE_SENSOR) {
      float value = NAN;
      if (record.payload == PAYLOAD_WORD) {
        uint32_t bits = encode_uint32(record.data[3], record.data[2], record.data[1], record.data[0]);
        memcpy(&value, &bits, sizeof(value));
      }
      for (auto &remote : this->remote_sensors_) {
        if (remote.known && remote.dictionary_id == dictionary_id && remote.number == record.number) {
          remote.last_state_ms = now;
          remote.has_state = true;
          remote.entity->publish_state(value);
        }
      }
    }
#endif
#ifdef ENTITY_TRANSPORT_REMOTE_BINARY_SENSOR_COUNT
    if (record.type == TYPE_BINARY_SENSOR) {
      for (auto &remote : this->remote_binary_sensors_) {
        if (!remote.known || remote.dictionary_id != dictionary_id || remote.number != record.number)
          continue;
        remote.last_state_ms = now;
        remote.has_state = true;
        if (record.payload == PAYLOAD_NO_STATE) {
          remote.entity->invalidate_state();
        } else {
          remote.entity->publish_state(record.value);
        }
      }
    }
#endif
  }
}

void EntityTransport::request_dictionary_(uint32_t dictionary_id) {
  uint32_t now = App.get_loop_component_start_time();
  if (this->request_sent_ && now - this->last_request_ms_ < REQUEST_INTERVAL_MS)
    return;
  uint8_t packet[HEADER_SIZE] = {PACKET_REQUEST};
  put_uint32(packet + 1, dictionary_id);
  // A refused request is tried again with the next data packet under that dictionary id
  if (!this->interface_->send_to_interface(PacketBuffer(packet, sizeof(packet)), {})) {
    ESP_LOGV(TAG, "Request for dictionary %08" PRIX32 " not accepted", dictionary_id);
    return;
  }
  this->request_sent_ = true;
  this->last_request_ms_ = now;
  ESP_LOGD(TAG, "Requested dictionary %08" PRIX32, dictionary_id);
}

// Dropped packets point to a device with other firmware or a fault, and may arrive steadily, so warn only now and then
bool EntityTransport::warning_due_() {
  uint32_t now = App.get_loop_component_start_time();
  if (this->warned_ && now - this->last_warning_ms_ < WARNING_INTERVAL_MS)
    return false;
  this->warned_ = true;
  this->last_warning_ms_ = now;
  return true;
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
