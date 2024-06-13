#include "gpio_one_wire.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace gpio {

static const char *const TAG = "gpio.one_wire";

void GPIOOneWireBus::setup() {
  ESP_LOGCONFIG(TAG, "Setting up 1-wire bus...");
  this->search();
}

void GPIOOneWireBus::dump_config() {
  ESP_LOGCONFIG(TAG, "GPIO 1-wire bus:");
  LOG_PIN("  Pin: ", this->t_pin_);
  this->dump_devices_(TAG);
}

bool HOT IRAM_ATTR GPIOOneWireBus::reset() {
  // See reset here:
  // https://www.maximintegrated.com/en/design/technical-documents/app-notes/1/126.html
  // Wait for communication to clear (delay G)
  pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  uint8_t retries = 125;
  do {
    if (--retries == 0)
      return false;
    delayMicroseconds(2);
  } while (!pin_.digital_read());

  bool r;

  // Send 480µs LOW TX reset pulse (drive bus low, delay H)
  pin_.pin_mode(gpio::FLAG_OUTPUT);
  pin_.digital_write(false);
  delayMicroseconds(480);

  // Release the bus, delay I
  pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  delayMicroseconds(70);

  // sample bus, 0=device(s) present, 1=no device present
  r = !pin_.digital_read();
  // delay J
  delayMicroseconds(410);
  return r;
}

void HOT IRAM_ATTR GPIOOneWireBus::write_bit_(bool bit) {
  // drive bus low
  pin_.pin_mode(gpio::FLAG_OUTPUT);
  pin_.digital_write(false);

  // from datasheet:
  // write 0 low time: t_low0: min=60µs, max=120µs
  // write 1 low time: t_low1: min=1µs, max=15µs
  // time slot: t_slot: min=60µs, max=120µs
  // recovery time: t_rec: min=1µs
  // ds18b20 appears to read the bus after roughly 14µs
  uint32_t delay0 = bit ? 6 : 60;
  uint32_t delay1 = bit ? 54 : 5;

  // delay A/C
  delayMicroseconds(delay0);
  // release bus
  pin_.digital_write(true);
  // delay B/D
  delayMicroseconds(delay1);
}

bool HOT IRAM_ATTR GPIOOneWireBus::read_bit_() {
  // drive bus low
  pin_.pin_mode(gpio::FLAG_OUTPUT);
  pin_.digital_write(false);

  // note: for reading we'll need very accurate timing, as the
  // timing for the digital_read() is tight; according to the datasheet,
  // we should read at the end of 16µs starting from the bus low
  // typically, the ds18b20 pulls the line high after 11µs for a logical 1
  // and 29µs for a logical 0

  uint32_t start = micros();
  // datasheet says >1µs
  delayMicroseconds(2);

  // release bus, delay E
  pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);

  // measure from start value directly, to get best accurate timing no matter
  // how long pin_mode/delayMicroseconds took
  delayMicroseconds(12 - (micros() - start));

  // sample bus to read bit from peer
  bool r = pin_.digital_read();

  // read slot is at least 60µs; get as close to 60µs to spend less time with interrupts locked
  uint32_t now = micros();
  if (now - start < 60)
    delayMicroseconds(60 - (now - start));

  return r;
}

void IRAM_ATTR GPIOOneWireBus::write8(uint8_t val) {
  for (uint8_t i = 0; i < 8; i++) {
    this->write_bit_(bool((1u << i) & val));
  }
}

void IRAM_ATTR GPIOOneWireBus::write64(uint64_t val) {
  for (uint8_t i = 0; i < 64; i++) {
    this->write_bit_(bool((1ULL << i) & val));
  }
}

uint8_t IRAM_ATTR GPIOOneWireBus::read8() {
  uint8_t ret = 0;
  for (uint8_t i = 0; i < 8; i++) {
    ret |= (uint8_t(this->read_bit_()) << i);
  }
  return ret;
}

uint64_t IRAM_ATTR GPIOOneWireBus::read64() {
  uint64_t ret = 0;
  for (uint8_t i = 0; i < 8; i++) {
    ret |= (uint64_t(this->read_bit_()) << i);
  }
  return ret;
}

void GPIOOneWireBus::reset_search() {
  this->last_discrepancy_ = 0;
  this->last_device_flag_ = false;
  this->address_ = 0;
}

uint64_t IRAM_ATTR GPIOOneWireBus::search_int() {
  if (this->last_device_flag_)
    return 0u;

  uint8_t last_zero = 0;
  uint64_t bit_mask = 1;
  uint64_t address = this->address_;

  // Initiate search
  for (int bit_number = 1; bit_number <= 64; bit_number++, bit_mask <<= 1) {
    // read bit
    bool id_bit = this->read_bit_();
    // read its complement
    bool cmp_id_bit = this->read_bit_();

    if (id_bit && cmp_id_bit) {
      // No devices participating in search
      return 0;
    }

    bool branch;

    if (id_bit != cmp_id_bit) {
      // only chose one branch, the other one doesn't have any devices.
      branch = id_bit;
    } else {
      // there are devices with both 0s and 1s at this bit
      if (bit_number < this->last_discrepancy_) {
        branch = (address & bit_mask) > 0;
      } else {
        branch = bit_number == this->last_discrepancy_;
      }

      if (!branch) {
        last_zero = bit_number;
      }
    }

    if (branch) {
      address |= bit_mask;
    } else {
      address &= ~bit_mask;
    }

    // choose/announce branch
    this->write_bit_(branch);
  }

  this->last_discrepancy_ = last_zero;
  if (this->last_discrepancy_ == 0) {
    // we're at root and have no choices left, so this was the last one.
    this->last_device_flag_ = true;
  }

  this->address_ = address;
  return address;
}

}  // namespace gpio
}  // namespace esphome
