#include "esp_one_wire.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace dallas {

static const char *const TAG = "dallas.one_wire";

const uint8_t ONE_WIRE_ROM_SELECT = 0x55;
const int ONE_WIRE_ROM_SEARCH = 0xF0;

ESPOneWire::ESPOneWire(InternalGPIOPin *pin) { pin_ = pin->to_isr(); }

bool HOT IRAM_ATTR ESPOneWire::reset() {
  // See reset here:
  // https://www.maximintegrated.com/en/design/technical-documents/app-notes/1/126.html
  InterruptLock lock;

  // Wait for communication to clear (delay G)
  pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  uint8_t retries = 125;
  do {
    if (--retries == 0)
      return false;
    delayMicroseconds(2);
  } while (!pin_.digital_read());

  // Send 480µs LOW TX reset pulse (drive bus low, delay H)
  pin_.pin_mode(gpio::FLAG_OUTPUT);
  pin_.digital_write(false);
  delayMicroseconds(480);

  // Release the bus, delay I
  pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  delayMicroseconds(70);

  // sample bus, 0=device(s) present, 1=no device present
  bool r = !pin_.digital_read();
  // delay J
  delayMicroseconds(410);
  return r;
}

void HOT IRAM_ATTR ESPOneWire::write_bit(bool bit) {
  // See write 1/0 bit here:
  // https://www.maximintegrated.com/en/design/technical-documents/app-notes/1/126.html
  InterruptLock lock;

  // drive bus low
  pin_.pin_mode(gpio::FLAG_OUTPUT);
  pin_.digital_write(false);

  uint32_t delay0 = bit ? 10 : 65;
  uint32_t delay1 = bit ? 55 : 5;

  // delay A/C
  delayMicroseconds(delay0);
  // release bus
  pin_.digital_write(true);
  // delay B/D
  delayMicroseconds(delay1);
}

bool HOT IRAM_ATTR ESPOneWire::read_bit() {
  // See read bit here:
  // https://www.maximintegrated.com/en/design/technical-documents/app-notes/1/126.html
  InterruptLock lock;

  // drive bus low, delay A
  pin_.pin_mode(gpio::FLAG_OUTPUT);
  pin_.digital_write(false);
  delayMicroseconds(3);

  // release bus, delay E
  pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  delayMicroseconds(10);

  // sample bus to read bit from peer
  bool r = pin_.digital_read();

  // delay F
  delayMicroseconds(53);
  return r;
}

void ESPOneWire::write8(uint8_t val) {
  for (uint8_t i = 0; i < 8; i++) {
    this->write_bit(bool((1u << i) & val));
  }
}

void ESPOneWire::write64(uint64_t val) {
  for (uint8_t i = 0; i < 64; i++) {
    this->write_bit(bool((1ULL << i) & val));
  }
}

uint8_t ESPOneWire::read8() {
  uint8_t ret = 0;
  for (uint8_t i = 0; i < 8; i++) {
    ret |= (uint8_t(this->read_bit()) << i);
  }
  return ret;
}
uint64_t ESPOneWire::read64() {
  uint64_t ret = 0;
  for (uint8_t i = 0; i < 8; i++) {
    ret |= (uint64_t(this->read_bit()) << i);
  }
  return ret;
}
void ESPOneWire::select(uint64_t address) {
  this->write8(ONE_WIRE_ROM_SELECT);
  this->write64(address);
}
void ESPOneWire::reset_search() {
  this->last_discrepancy_ = 0;
  this->last_device_flag_ = false;
  this->last_family_discrepancy_ = 0;
  this->rom_number_ = 0;
}
uint64_t ESPOneWire::search() {
  if (this->last_device_flag_) {
    return 0u;
  }

  if (!this->reset()) {
    // Reset failed or no devices present
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
std::vector<uint64_t> ESPOneWire::search_vec() {
  std::vector<uint64_t> res;

  this->reset_search();
  uint64_t address;
  while ((address = this->search()) != 0u)
    res.push_back(address);

  return res;
}
void ESPOneWire::skip() {
  this->write8(0xCC);  // skip ROM
}

uint8_t IRAM_ATTR *ESPOneWire::rom_number8_() { return reinterpret_cast<uint8_t *>(&this->rom_number_); }

}  // namespace dallas
}  // namespace esphome
