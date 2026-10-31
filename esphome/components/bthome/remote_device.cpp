#include "remote_device.h"
#include "decoder.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::bthome::client {

ESPHOME_LOG_TAG(TAG, "bthome");

static constexpr size_t BITS_PER_BYTE = 8;

size_t RemoteDeviceBase::process_object_(std::span<BTHomeRemoteObject *> handlers, size_t index,
                                         const BTHomeObject &object) {
  for (size_t i = index; i < handlers.size(); i++) {
    if (handlers[i]->process_object(object))
      return i + 1;
  }
  return index;
}

// Reads a MAC address embedded in a payload, least significant byte first.
static uint64_t read_embedded_mac(const uint8_t *data) {
  uint64_t address = 0;
  for (size_t i = MAC_ADDRESS_SIZE; i > 0; i--)
    address = (address << BITS_PER_BYTE) | data[i - 1];
  return address;
}

bool RemoteDeviceBase::matches_(const ble_device_base::ESPBTDevice &device, const uint8_t *data,
                                size_t data_size) const {
  if (device.address_uint64() == this->address_)
    return true;
  // Devices with a changing BLE address can embed their MAC. It is only readable before decryption
  // when the frame is not encrypted.
  if (data_size < sizeof(BTHomeHeader) + MAC_ADDRESS_SIZE)
    return false;
  const BTHomeHeader header{data[0]};
  return header.mac_included() && !header.encrypted() &&
         read_embedded_mac(data + sizeof(BTHomeHeader)) == this->address_;
}

// The spec asks for a new packet ID whenever the data changes, but some firmware (pvvx ATC_MiThermometer)
// alternates several different payloads under one packet ID. Only a payload already seen under the
// current packet ID is treated as a duplicate. Without a packet ID only a copy of the previous frame is,
// and only while the copies arrive within NO_PACKET_ID_REPEAT_MS of each other.
bool RemoteDeviceBase::is_duplicate_(optional<uint8_t> packet_id, const uint8_t *payload, size_t payload_size,
                                     uint32_t now) {
  uint32_t hash = FNV1_OFFSET_BASIS;
  for (size_t i = 0; i < payload_size; i++)
    hash = fnv1a_hash_extend(hash, payload[i]);

  // A restarted transmitter can send the same packet ID again, so a pause forgets what was seen.
  const uint32_t window = packet_id.has_value() ? PACKET_ID_RESET_MS : NO_PACKET_ID_REPEAT_MS;
  const bool pause = now - this->last_frame_ms_ >= window;
  this->last_frame_ms_ = now;
  if (this->last_packet_id_ != packet_id || pause) {
    this->last_packet_id_ = packet_id;
    this->seen_payload_count_ = 0;
    this->next_seen_payload_ = 0;
  }
  for (size_t i = 0; i < this->seen_payload_count_; i++) {
    if (this->seen_payload_hashes_[i] == hash)
      return true;
  }
  // A value that comes back without a packet ID is new data, so only this frame is kept.
  if (!packet_id.has_value()) {
    this->seen_payload_count_ = 0;
    this->next_seen_payload_ = 0;
  }
  this->seen_payload_hashes_[this->next_seen_payload_] = hash;
  this->next_seen_payload_ = (this->next_seen_payload_ + 1) % SEEN_PAYLOAD_SLOTS;
  if (this->seen_payload_count_ < SEEN_PAYLOAD_SLOTS)
    this->seen_payload_count_++;
  return false;
}

bool RemoteDeviceBase::parse_data(const ble_device_base::ESPBTDevice &device, const uint8_t *data, size_t data_size) {
  if (!this->matches_(device, data, data_size)) {
    return false;
  }
  if (data_size == 0) {
    ESP_LOGVV(TAG, "BTHome data is empty");
    return true;
  }

  const BTHomeHeader header{data[0]};

  const uint8_t *payload;
  size_t payload_size;

  if (header.encrypted()) {
    if (!this->encryption_mismatch_logged_) {
      this->encryption_mismatch_logged_ = true;
      char addr_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
      ESP_LOGW(TAG, "Encrypted BTHome frames are not supported yet, ignoring %s", device.address_str_to(addr_buf));
    }
    return true;
  }
  payload = data + 1;
  payload_size = data_size - 1;

  if (header.mac_included()) {
    if (payload_size < MAC_ADDRESS_SIZE) {
      ESP_LOGVV(TAG, "BTHome payload too short for its MAC address");
      return true;
    }
    payload += MAC_ADDRESS_SIZE;
    payload_size -= MAC_ADDRESS_SIZE;
  }

  BTHomePayloadDecoder decoder(payload, payload_size);

  // Ignore repeated frames. The packet ID (object type 0x00) is optional; since objects are in
  // ascending order, it always appears first if present.
  auto it = decoder.begin();
  optional<uint8_t> packet_id;
  if (it != decoder.end() && (*it).type == BTHomeObjectType::PACKET_ID)
    packet_id = (*it).data[0];
  if (this->is_duplicate_(packet_id, payload, payload_size, App.get_loop_component_start_time())) {
    char addr_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
    ESP_LOGVV(TAG, "Repeated frame from %s, ignoring", device.address_str_to(addr_buf));
    return true;
  }

  size_t index = 0;
  auto handlers = this->get_handlers();
  for (const BTHomeObject &obj : decoder) {
    index = this->process_object_(handlers, index, obj);
  }

  const uint8_t rssi = static_cast<uint8_t>(device.get_rssi());
  const BTHomeObject signal_strength{BTHomeObjectType::SIGNAL_STRENGTH, &rssi, sizeof(rssi)};
  this->process_object_(handlers, index, signal_strength);

  return true;
}

void RemoteDeviceBase::dump_config() {
  // address_ holds the MAC most significant byte first, in its low 48 bits.
  uint8_t mac[MAC_ADDRESS_SIZE];
  for (size_t i = 0; i < MAC_ADDRESS_SIZE; i++)
    mac[i] = static_cast<uint8_t>(this->address_ >> ((MAC_ADDRESS_SIZE - 1 - i) * BITS_PER_BYTE));
  char addr_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  format_mac_addr_upper(mac, addr_buf);
  ESP_LOGCONFIG(TAG, "  Remote device %s", addr_buf);
  for (BTHomeRemoteObject *handler : this->get_handlers())
    handler->log_config();
}

void dump_remote_devices(std::span<RemoteDeviceBase *const> devices) {
  ESP_LOGCONFIG(TAG, "BTHome:");
  for (RemoteDeviceBase *device : devices) {
    if (device != nullptr)
      device->dump_config();
  }
}

}  // namespace esphome::bthome::client
