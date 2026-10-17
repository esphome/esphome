#include "bthome_binary_sensor.h"

#include "codec.h"

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome::bthome {

ESPHOME_LOG_TAG(TAG, "bthome.button");
// Service-data UUID 0xFCD2, little-endian, the same pair mithermometer matches.
static constexpr uint8_t BTHOME_UUID_LO = 0xD2;
static constexpr uint8_t BTHOME_UUID_HI = 0xFC;

void BTHomeButtonBinarySensor::setup() { this->publish_initial_state(false); }

void BTHomeButtonBinarySensor::dump_config() {
  LOG_BINARY_SENSOR("", "BTHome Button", this);
  uint8_t mac[MAC_ADDRESS_SIZE];
  ble_device_base::uint64_to_mac_msb_first(this->address_, mac);
  char addr[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  ESP_LOGCONFIG(TAG,
                "  Address: %s\n"
                "  Index: %u\n"
                "  Event: 0x%02X\n"
                "  Pulse: %" PRIu32 " ms",
                format_mac_addr_upper(mac, addr), this->index_, this->event_, this->pulse_length_ms_);
}

bool BTHomeButtonBinarySensor::parse_device(const ble_device_base::ESPBTDevice &device) {
  const bool address_matches = device.address_uint64() == this->address_;
  bool matched = false;
  for (const auto &service_data : device.get_service_datas()) {
    if (!service_data.uuid.contains(BTHOME_UUID_LO, BTHOME_UUID_HI)) {
      continue;
    }
    codec::Parsed parsed{};
    const bool valid = codec::parse(service_data.data.data(), service_data.data.size(), &parsed);
    // A MAC in the payload names a transmitter whose radio address changes.
    if (!address_matches &&
        (!parsed.has_mac || ble_device_base::mac_lsb_first_to_uint64(parsed.mac) != this->address_)) {
      continue;
    }
    matched = true;
    if (!valid) {
      if (parsed.encrypted && !this->encrypted_logged_) {
        this->encrypted_logged_ = true;
        char addr[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
        ESP_LOGW(TAG, "Encrypted advertisement from %s ignored", device.address_str_to(addr));
      }
      continue;
    }
    const uint32_t now = App.get_loop_component_start_time();
    // The packet id is compared with the previous advertisement, whatever gesture it carried.
    if (parsed.has_packet_id && !this->dedup_.new_packet(parsed.packet_id, now)) {
      continue;
    }
    const uint8_t event = button_event_at(parsed, this->index_);
    if (event == 0 || event != this->event_) {
      continue;
    }
    if (!parsed.has_packet_id && !this->dedup_.new_gesture(now)) {
      continue;
    }
    this->publish_state(true);
    this->set_timeout("pulse", this->pulse_length_ms_, [this]() { this->publish_state(false); });
  }
  return matched;
}

}  // namespace esphome::bthome
