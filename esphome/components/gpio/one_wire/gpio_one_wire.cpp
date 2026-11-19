#include "gpio_one_wire.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::gpio {

ESPHOME_LOG_TAG(TAG, "gpio.one_wire");

void GPIOOneWireBus::setup() {
  if (!this->t_pin_->setup_open_drain(true)) {
    this->mark_failed(LOG_STR("Cannot configure open-drain pin"));
    return;
  }
  // clear bus with 480µs high, otherwise initial reset in search might fail
  delayMicroseconds(480);
  this->search();
}

void GPIOOneWireBus::dump_config() {
  ESP_LOGCONFIG(TAG, "GPIO 1-wire bus:");
  LOG_PIN("  Pin: ", this->t_pin_);
  this->dump_devices_(TAG);
}

// Lock only what must not stretch; the ESP8266 Wi-Fi interrupt handler asserts (hardware WDT) when held off ~1 ms.
// A longer gap between slots is within spec.
int HOT IRAM_ATTR GPIOOneWireBus::reset_int() {
  // See reset here:
  // https://www.maximintegrated.com/en/design/technical-documents/app-notes/1/126.html
  // Wait for communication to clear (delay G)
  uint8_t retries = 125;
  do {
    if (--retries == 0)
      return -1;
    delayMicroseconds(2);
  } while (!this->pin_.open_drain_read());

  bool r = false;
  uint32_t start;
  {
    // Locked from the reset pulse through presence detection (at most about 780 us): a reset pulse stretched past
    // 960 us power-resets a parasite-powered sensor
    InterruptLock lock;
    // Send 480µs LOW TX reset pulse (drive bus low, delay H)
    this->pin_.open_drain_pull_low();
    delayMicroseconds(480);

    // Release the bus, delay I
    this->pin_.open_drain_release();
    start = micros();
    delayMicroseconds(30);

    while (micros() - start < 300) {
      // sample bus, 0=device(s) present, 1=no device present
      r = !this->pin_.open_drain_read();
      if (r)
        break;
      delayMicroseconds(1);
    }
  }

  // delay J: finish the 480us slot, but never spin if it already elapsed (unsigned wrap)
  uint32_t elapsed = micros() - start;
  if (elapsed < 480)
    delayMicroseconds(480 - elapsed);
  this->pin_.open_drain_release();
  return r ? 1 : 0;
}

void HOT IRAM_ATTR GPIOOneWireBus::write_bit_(bool bit) {
  // from datasheet:
  // write 0 low time: t_low0: min=60µs, max=120µs
  // write 1 low time: t_low1: min=1µs, max=15µs
  // time slot: t_slot: min=60µs, max=120µs
  // recovery time: t_rec: min=1µs
  // ds18b20 appears to read the bus after roughly 14µs
  uint32_t delay0 = bit ? 6 : 60;
  uint32_t delay1 = bit ? 64 : 10;

  {
    InterruptLock lock;
    // drive bus low
    this->pin_.open_drain_pull_low();
    // delay A/C
    delayMicroseconds(delay0);
    // release bus
    this->pin_.open_drain_release();
  }
  // delay B/D
  delayMicroseconds(delay1);
}

bool HOT IRAM_ATTR GPIOOneWireBus::read_bit_() {
  bool r;
  {
    InterruptLock lock;
    // drive bus low
    this->pin_.open_drain_pull_low();

    // datasheet says >= 1µs
    delayMicroseconds(5);

    // release bus, delay E
    this->pin_.open_drain_release();

    delayMicroseconds(8);
    // sample bus to read bit from peer
    r = this->pin_.open_drain_read();
  }

  // read slot is at least 60µs
  delayMicroseconds(50);

  this->pin_.open_drain_release();
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
  for (uint8_t i = 0; i < 8; i++)
    ret |= (uint8_t(this->read_bit_()) << i);
  return ret;
}

uint64_t IRAM_ATTR GPIOOneWireBus::read64() {
  uint64_t ret = 0;
  for (uint8_t i = 0; i < 64; i++) {
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

}  // namespace esphome::gpio
