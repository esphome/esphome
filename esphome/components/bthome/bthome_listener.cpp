#include "bthome_listener.h"
#include "mbedtls/ccm.h"
#include "bthome_decoder.h"

#include <cstring>

namespace esphome {
namespace bthome {

MacAddress::MacAddress(const uint8_t *addr) { *this = addr; }

MacAddress::MacAddress(uint64_t addr) {
  for (int i = sizeof(this->addr_) - 1; i >= 0; i--) {
    this->addr_[i] = addr & 0xFF;
    addr >>= 8;
  }
}

MacAddress &MacAddress::operator=(const uint8_t *addr) {
  std::memcpy(this->addr_, addr, sizeof(this->addr_));
  return *this;
}

MacAddress::operator const uint8_t *() const { return this->addr_; }

bool MacAddress::operator==(const uint8_t *other) const {
  return std::memcmp(this->addr_, other, sizeof(this->addr_)) == 0;
}
bool MacAddress::operator==(const MacAddress &other) const { return *this == static_cast<const uint8_t *>(other); }
bool MacAddress::operator!=(const uint8_t *other) const { return !(*this == other); }
bool MacAddress::operator!=(const MacAddress &other) const { return !(*this == other); }

bool MacAddressPtr::operator==(const uint8_t *other) const {
  return std::memcmp(this->addr_, other, MAC_ADDRESS_SIZE) == 0;
}
bool MacAddressPtr::operator==(const MacAddressPtr &other) const { return *this == other.addr_; }
bool MacAddressPtr::operator!=(const uint8_t *other) const { return !(*this == other); }
bool MacAddressPtr::operator!=(const MacAddressPtr &other) const { return !(*this == other); }

const char *MacAddressPtr::c_str() const {
  static char buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  format_mac_addr_upper(this->addr_, buf);
  return buf;
}

const char *MacAddress::c_str() const { return MacAddress(*this).c_str(); }

bool DeviceBase::parse_data(MacAddressPtr source_address, const uint8_t *data, size_t data_size) {
  if (this->address_ != source_address) {
    ESP_LOGD(TAG, "not interested");
    return false;
  }
  BTHomeHeader &header = *(BTHomeHeader *) &data[0];

  if (header.encrypted && !this->encryption_key.has_value()) {
    ESP_LOGE(TAG, "Encrypted BTHome frame received but no bindkey configured for %s", source_address.c_str());
    return false;
  }

  if (header.encrypted && this->encryption_key.has_value()) {
    ESP_LOGE(TAG, "Unencrypted BTHome frame received with bindkey configured for %s", source_address.c_str());
    return false;
  }

  if (header.encrypted) {
    ESP_LOGE(TAG, "Encryption not supported yet");
    return false;
  }

  const uint8_t *payload = data + 1;
  size_t payload_size = data_size - 1;

  BTHomePayloadDecoder decoder(payload, payload_size);

  int index = 0;
  auto sensors = this->get_sensors();
  for (const BTHomeObject &obj : decoder) {
    for (int i = index; i < sensors.size(); i++) {
      BTHomeSensorBase *sensor = sensors[i];
      if (sensor->process_object(obj)) {
        index++;
        break;
      }
    }
  }
  return true;

  /*
  std::vector<uint8_t> decrypted_payload;
  const uint8_t *payload = nullptr;
  size_t payload_size = 0;

  if (is_encrypted) {
    if (!this->decrypt_bthome_payload_(data, source_address, decrypted_payload)) {
      char addr_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
      ESP_LOGVV(TAG, "Failed to decrypt BTHome frame from %s", device.address_str_to(addr_buf));
      return false;
    }
    payload = decrypted_payload.data();
    payload_size = decrypted_payload.size();
  } else {
    payload = data.data() + 1;
    payload_size = data.size() - 1;
  }

  if (mac_included) {
    if (payload_size < 6) {
      ESP_LOGVV(TAG, "BTHome payload missing MAC address");
      return false;
    }
    source_address = 0;
    for (int i = 5; i >= 0; i--) {
      source_address = (source_address << 8) | payload[i];
    }
    payload += 6;
    payload_size -= 6;
  }

  char addr_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  if (source_address != this->address_) {
    ESP_LOGVV(TAG, "BTHome frame from unexpected device %s", format_mac_address(addr_buf, source_address));
    return false;
  }

  if (payload_size == 0) {
    ESP_LOGVV(TAG, "BTHome payload empty after header");
    return false;
  }
  */
  return false;
}
bool BTHomeSensorBase::process_object(const BTHomeObject &object) {
  auto object_types = this->get_objects();
  for (BTHomeObjectType object_type : object_types) {
    if (object.type == object_type) {
      this->publish_state(object.as_float());
      return true;
    }
  }
  return false;
}
}  // namespace bthome
}  // namespace esphome
