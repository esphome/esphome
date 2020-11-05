#include "esp_one_wire.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace dallas {

static const char *TAG = "dallas.one_wire";

const uint8_t ONE_WIRE_ROM_SELECT = 0x55;
const int ONE_WIRE_ROM_SEARCH = 0xF0;

ESPOneWire::ESPOneWire(GPIOPin *pin) : pin_(pin) {}

bool HOT ICACHE_RAM_ATTR ESPOneWire::reset() {
  uint8_t retries = 125;

  // Wait for communication to clear
  this->pin_->pin_mode(INPUT_PULLUP);
  do {
    if (--retries == 0)
      return false;
    delayMicroseconds(2);
  } while (!this->pin_->digital_read());

  // Send 480µs LOW TX reset pulse
  this->pin_->pin_mode(OUTPUT);
  this->pin_->digital_write(false);
  delayMicroseconds(480);

  // Switch into RX mode, letting the pin float
  this->pin_->pin_mode(INPUT_PULLUP);
  // after 15µs-60µs wait time, responder pulls low for 60µs-240µs
  // let's have 70µs just in case
  delayMicroseconds(70);

  bool r = !this->pin_->digital_read();
  delayMicroseconds(410);
  return r;
}

void HOT ICACHE_RAM_ATTR ESPOneWire::write_bit(bool bit) {
  // Initiate write/read by pulling low.
  this->pin_->pin_mode(OUTPUT);
  this->pin_->digital_write(false);

  // bus sampled within 15µs and 60µs after pulling LOW.
  if (bit) {
    // pull high/release within 15µs
    delayMicroseconds(10);
    this->pin_->digital_write(true);
    // in total minimum of 60µs long
    delayMicroseconds(55);
  } else {
    // continue pulling LOW for at least 60µs
    delayMicroseconds(65);
    this->pin_->digital_write(true);
    // grace period, 1µs recovery time
    delayMicroseconds(5);
  }
}

bool HOT ICACHE_RAM_ATTR ESPOneWire::read_bit() {
  // Initiate read slot by pulling LOW for at least 1µs
  this->pin_->pin_mode(OUTPUT);
  this->pin_->digital_write(false);
  delayMicroseconds(3);

  // release bus, we have to sample within 15µs of pulling low
  this->pin_->pin_mode(INPUT_PULLUP);
  delayMicroseconds(10);

  bool r = this->pin_->digital_read();
  // read time slot at least 60µs long + 1µs recovery time between slots
  delayMicroseconds(53);
  return r;
}

void ICACHE_RAM_ATTR ESPOneWire::write8(uint8_t val) {
  for (uint8_t i = 0; i < 8; i++) {
    this->write_bit(bool((1u << i) & val));
  }
}

void ICACHE_RAM_ATTR ESPOneWire::write64(uint64_t val) {
  for (uint8_t i = 0; i < 64; i++) {
    this->write_bit(bool((1ULL << i) & val));
  }
}

uint8_t ICACHE_RAM_ATTR ESPOneWire::read8() {
  uint8_t ret = 0;
  for (uint8_t i = 0; i < 8; i++) {
    ret |= (uint8_t(this->read_bit()) << i);
  }
  return ret;
}
uint64_t ICACHE_RAM_ATTR ESPOneWire::read64() {
  uint64_t ret = 0;
  for (uint8_t i = 0; i < 8; i++) {
    ret |= (uint64_t(this->read_bit()) << i);
  }
  return ret;
}
void ICACHE_RAM_ATTR ESPOneWire::select(uint64_t address) {
  this->write8(ONE_WIRE_ROM_SELECT);
  this->write64(address);
}
void ICACHE_RAM_ATTR ESPOneWire::reset_search() {
  this->last_discrepancy_ = 0;
  this->last_device_flag_ = false;
  this->last_family_discrepancy_ = 0;
  this->rom_number_ = 0;
}
uint64_t HOT ICACHE_RAM_ATTR ESPOneWire::search() {
  if (this->last_device_flag_) {
    return 0u;
  }

  if (!this->reset()) {
    // Reset failed
    this->reset_search();
    return 0u;
  }

  uint8_t id_bit_number = 1;
  uint8_t last_zero = 0;
  uint8_t rom_byte_number = 0;
  bool search_result = false;
  uint8_t rom_byte_mask = 1;

  // Initiate search
  this->write8(ONE_WIRE_ROM_SEARCH);
  do {
    // read bit
    bool id_bit = this->read_bit();
    // read its complement
    bool cmp_id_bit = this->read_bit();

    if (id_bit && cmp_id_bit)
      // No devices participating in search
      break;

    bool branch;

    if (id_bit != cmp_id_bit) {
      // only chose one branch, the other one doesn't have any devices.
      branch = id_bit;
    } else {
      // there are devices with both 0s and 1s at this bit
      if (id_bit_number < this->last_discrepancy_) {
        branch = (this->rom_number8_()[rom_byte_number] & rom_byte_mask) > 0;
      } else {
        branch = id_bit_number == this->last_discrepancy_;
      }

      if (!branch) {
        last_zero = id_bit_number;
        if (last_zero < 9) {
          this->last_discrepancy_ = last_zero;
        }
      }
    }

    if (branch)
      // set bit
      this->rom_number8_()[rom_byte_number] |= rom_byte_mask;
    else
      // clear bit
      this->rom_number8_()[rom_byte_number] &= ~rom_byte_mask;

    // choose/announce branch
    this->write_bit(branch);
    id_bit_number++;
    rom_byte_mask <<= 1;
    if (rom_byte_mask == 0u) {
      // go to next byte
      rom_byte_number++;
      rom_byte_mask = 1;
    }
  } while (rom_byte_number < 8);  // loop through all bytes

  if (id_bit_number >= 65) {
    this->last_discrepancy_ = last_zero;
    if (this->last_discrepancy_ == 0)
      // we're at root and have no choices left, so this was the last one.
      this->last_device_flag_ = true;
    search_result = true;
  }

  search_result = search_result && (this->rom_number8_()[0] != 0);
  if (!search_result) {
    this->reset_search();
    return 0u;
  }

  return this->rom_number_;
}
std::vector<uint64_t> ICACHE_RAM_ATTR ESPOneWire::search_vec() {
  std::vector<uint64_t> res;

  this->reset_search();
  uint64_t address;
  while ((address = this->search()) != 0u)
    res.push_back(address);

  return res;
}
void ICACHE_RAM_ATTR ESPOneWire::skip() {
  this->write8(0xCC);  // skip ROM
}
GPIOPin *ESPOneWire::get_pin() { return this->pin_; }

uint8_t ICACHE_RAM_ATTR *ESPOneWire::rom_number8_() { return reinterpret_cast<uint8_t *>(&this->rom_number_); }

}  // namespace dallas
}  // namespace esphome
