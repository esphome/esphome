#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "dallas_ibutton.h"

#include <cinttypes>

namespace esphome {
namespace dallas_ibutton {

static const char *const TAG = "dallas_ibutton";

static const uint8_t DALLAS_COMMAND_SEARCH_ROM = 0xF0;
static const uint8_t DALLAS_COMMAND_MATCH_ROM = 0x55;
static const uint8_t DALLAS_COMMAND_READ_MEMORY = 0xF0;
static const uint8_t DALLAS_COMMAND_WRITE_SCRATCHPAD = 0x0F;
static const uint8_t DALLAS_COMMAND_READ_SCRATCH_PAD = 0xAA;
static const uint8_t DALLAS_COMMAND_COPY_SCRATCH_PAD = 0x55;

void DallasIbuttonComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Dallas iButton Configuration:\n"
                "  Reset Value After: %d ms\n"
                "  Update Interval: %ums\n",
                this->reset_value_after_, this->update_interval_);
}

void DallasIbuttonComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Dallas iButton Component...");
  if (this->bus_ == nullptr) {
    ESP_LOGE(TAG, "Failed to initialize OneWire bus");
    this->mark_failed();
    return;
  } else {
    ESP_LOGCONFIG(TAG, "Successfully initialized OneWire bus");
  }

  this->publish_state("");
}

void DallasIbuttonComponent::update() {
  this->status_clear_warning();

  if (this->bus_ == nullptr) {
    ESP_LOGW(TAG, "One-Wire bus pointer is null – skipping scan");
    return;
  }

  this->bus_->search();

  const std::vector<uint64_t> &devices = this->bus_->get_devices();

  if (devices.empty()) {
    return;
  }

  for (uint64_t address : devices) {
    uint8_t *address_array = reinterpret_cast<uint8_t *>(&address);

    if (strcmp(this->get_device_type(address_array[0]), "Unknown") == 0) {
      ESP_LOGV(TAG, "Skipping non-iButton device 0x%02X on bus", address_array[0]);
      continue;
    }

    if (crc8(address_array, 7) != address_array[7]) {
      ESP_LOGW(TAG, "CRC error for device 0x%02X!", address_array[0]);
      continue;
    }

    char address_string[17];
    snprintf(address_string, sizeof(address_string), "%02X%02X%02X%02X%02X%02X%02X%02X", address_array[0],
             address_array[1], address_array[2], address_array[3], address_array[4], address_array[5], address_array[6],
             address_array[7]);

    if (strcmp(address_string, this->last_address_) == 0) {
      ESP_LOGV(TAG, "Device 0x%02X is the same as the last processed device", address_array[0]);
      continue;
    }

    ESP_LOGD(TAG, "Found iButton: %s (Type: %s)", address_string, this->get_device_type(address_array[0]));
    this->publish_state(address_string);
    strcpy(this->last_address_, address_string);
    break;
  }

  this->last_timestamp_ = App.get_loop_component_start_time();
}

void DallasIbuttonComponent::loop() {
  uint32_t now = App.get_loop_component_start_time();

  if (strlen(this->last_address_) > 0 && (now - this->last_timestamp_) > this->reset_value_after_) {
    ESP_LOGD(TAG, "Presence timeout expired (%u ms), clearing sensors", this->reset_value_after_);

    this->publish_state("");

    strcpy(this->last_address_, "");
    this->last_timestamp_ = now;
  }
}

const char *DallasIbuttonComponent::get_device_type(uint8_t family_code) {
  switch (family_code) {
    case 0x01:
      return "DS1990";  // Serial Number       | checked | DS1990A, DS1990R
    case 0x02:
      return "DS1991";  // MultiKey
    case 0x04:
      return "DS1994";  // 4Kb Memory + Clock  |
    case 0x06:
      return "DS1993";  // 4Kb Memory
    case 0x08:
      return "DS1992";  // 1Kb Memory          | checked
    case 0x09:
      return "DS1982";  // 1Kb EPROM           | checked
    case 0x0A:
      return "DS1995";  // 16Kb Memory
    case 0x0B:
      return "DS1985";  // 16Kb EPROM
    case 0x0C:
      return "DS1996";  // 64Kb Memory         | checked
    case 0x0F:
      return "DS1986";  // 64Kb EPROM
    case 0x12:
      return "DS2406";  // Dual Switch
    case 0x14:
      return "DS1971";  // 256-bit EEPROM      | checked
    case 0x18:
      return "DS1963S";  // SHA-1               | checked
    case 0x1D:
      return "DS2423";  // 4Kb Counter
    case 0x21:
      return "DS1921";  // Thermochron
    case 0x23:
      return "DS1973";  // 4Kb EEPROM          | checked
    case 0x24:
      return "DS1904";  // RTC
    case 0x89:
      return "DS1982U";  // 1Kb UniqueWare
    default:
      return "Unknown";
  }
}

}  // namespace dallas_ibutton
}  // namespace esphome
