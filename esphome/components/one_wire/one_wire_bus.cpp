#include "one_wire_bus.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace one_wire {

static const char *const TAG = "one_wire";

static const uint8_t DALLAS_MODEL_DS18S20 = 0x10;
static const uint8_t DALLAS_MODEL_DS1822 = 0x22;
static const uint8_t DALLAS_MODEL_DS18B20 = 0x28;
static const uint8_t DALLAS_MODEL_DS1825 = 0x3B;
static const uint8_t DALLAS_MODEL_DS28EA00 = 0x42;

const uint8_t ONE_WIRE_ROM_SELECT = 0x55;
const uint8_t ONE_WIRE_ROM_SEARCH = 0xF0;

const std::vector<uint64_t> &OneWireBus::get_devices() { return this->devices_; }

bool OneWireBus::reset_() {
  int res = this->reset_int();
  if (res == -1)
    ESP_LOGE(TAG, "1-wire bus is held low");
  return res == 1;
}

bool IRAM_ATTR OneWireBus::select(uint64_t address) {
  if (!this->reset_())
    return false;
  this->write8(ONE_WIRE_ROM_SELECT);
  this->write64(address);
  return true;
}

void OneWireBus::search() {
  this->devices_.clear();

  this->reset_search();
  uint64_t address;
  while (true) {
    if (!this->reset_()) {
      // Reset failed or no devices present
      return;
    }

    this->write8(ONE_WIRE_ROM_SEARCH);
    address = this->search_int();
    if (address == 0)
      break;
    auto *address8 = reinterpret_cast<uint8_t *>(&address);
    if (crc8(address8, 7) != address8[7]) {
      ESP_LOGW(TAG, "Dallas device 0x%s has invalid CRC.", format_hex(address).c_str());
    } else {
      this->devices_.push_back(address);
    }
  }
}

void OneWireBus::skip() {
  this->write8(0xCC);  // skip ROM
}

const LogString *OneWireBus::get_model_str(uint8_t model) {
  switch (model) {
    case DALLAS_MODEL_DS18S20:
      return LOG_STR("DS18S20");
    case DALLAS_MODEL_DS1822:
      return LOG_STR("DS1822");
    case DALLAS_MODEL_DS18B20:
      return LOG_STR("DS18B20");
    case DALLAS_MODEL_DS1825:
      return LOG_STR("DS1825");
    case DALLAS_MODEL_DS28EA00:
      return LOG_STR("DS28EA00");
    default:
      return LOG_STR("Unknown");
  }
}

void OneWireBus::dump_devices_(const char *tag) {
  if (this->devices_.empty()) {
    ESP_LOGW(tag, "  Found no devices!");
  } else {
    ESP_LOGCONFIG(tag, "  Found devices:");
    for (auto &address : this->devices_) {
      ESP_LOGCONFIG(tag, "    0x%s (%s)", format_hex(address).c_str(), LOG_STR_ARG(get_model_str(address & 0xff)));
    }
  }
}

}  // namespace one_wire
}  // namespace esphome
