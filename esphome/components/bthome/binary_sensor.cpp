// clang-tidy compiles this file without the platform define. The host
// build includes the header from esphome.h and must not pull binary_sensor.
#ifndef USE_BTHOME_BINARY_SENSOR
#define USE_BTHOME_BINARY_SENSOR
#endif

#include "binary_sensor.h"

#include "codec.h"

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome::bthome {

static const char *const TAG = "bthome.button";
// Service-data UUID 0xFCD2, little-endian, the same pair mithermometer matches.
static constexpr uint8_t BTHOME_UUID_LO = 0xD2;
static constexpr uint8_t BTHOME_UUID_HI = 0xFC;

// address_uint64() stores the first printed byte in the top of the word.
static void format_stored_address(uint64_t address, char *buf) {
  uint8_t mac[6];
  for (uint8_t i = 0; i < 6; i++) {
    mac[i] = static_cast<uint8_t>((address >> ((5 - i) * 8)) & 0xff);
  }
  format_mac_addr_upper(mac, buf);
}

void BTHomeButtonBinarySensor::setup() { this->publish_initial_state(false); }

void BTHomeButtonBinarySensor::dump_config() {
  LOG_BINARY_SENSOR("", "BTHome Button", this);
  char addr[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  format_stored_address(this->address_, addr);
  ESP_LOGCONFIG(TAG,
                "  Address: %s\n"
                "  Index: %u\n"
                "  Event: 0x%02X\n"
                "  Pulse: %" PRIu32 " ms",
                addr, this->index_, this->event_, this->pulse_length_ms_);
}

bool BTHomeButtonBinarySensor::parse_device(const ble_device_base::ESPBTDevice &device) {
  if (device.address_uint64() != this->address_) {
    return false;
  }
  bool matched = false;
  for (const auto &service_data : device.get_service_datas()) {
    if (!service_data.uuid.contains(BTHOME_UUID_LO, BTHOME_UUID_HI)) {
      continue;
    }
    matched = true;
    codec::Parsed parsed{};
    if (!codec::parse(service_data.data.data(), service_data.data.size(), &parsed)) {
      if (parsed.encrypted && !this->encrypted_logged_) {
        this->encrypted_logged_ = true;
        char addr[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
        format_stored_address(this->address_, addr);
        ESP_LOGW(TAG, "Encrypted advertisement from %s ignored", addr);
      }
      continue;
    }
    const uint8_t event = button_event_at(parsed, this->index_);
    if (event == 0 || event != this->event_) {
      continue;
    }
    if (!this->dedup_.accept(parsed.has_packet_id, parsed.packet_id, millis())) {
      continue;
    }
    this->publish_state(true);
    this->set_timeout("pulse", this->pulse_length_ms_, [this]() { this->publish_state(false); });
  }
  return matched;
}

}  // namespace esphome::bthome
