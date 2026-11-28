#include "device_model.h"

namespace esphome::one_wire_device {

void DeviceModel::configure(uint8_t family) {
  this->family_ = family;
#ifdef USE_ONE_WIRE_DEVICE_THERMOMETER
  this->pad_[0] = this->is_ds18s20_() ? 0xaa : 0x50;
  this->pad_[1] = this->is_ds18s20_() ? 0x00 : 0x05;
  this->pad_[2] = 75;
  this->pad_[3] = 70;
  this->pad_[4] = this->is_ds18s20_() ? 0xff : 0x7f;
  this->pad_[5] = 0xff;
  this->pad_[6] = 0x0c;
  this->pad_[7] = 0x10;
  for (uint8_t i = 0; i < 3; i++)
    this->saved_[i] = this->pad_[i + 2];
  this->update_crc_();
#endif
  this->reset();
}
void DeviceModel::set_rom(uint64_t address) {
  for (uint8_t i = 0; i < 8; i++)
    this->rom_[i] = static_cast<uint8_t>(address >> (8 * i));
}
void IRAM_ATTR DeviceModel::reset() {
  this->phase_ = Phase::ROM;
  this->rx_ = this->rx_bits_ = this->position_ = this->write_position_ = 0;
}
bool IRAM_ATTR DeviceModel::is_transmitting() const {
  return this->phase_ == Phase::READ_ROM || this->phase_ == Phase::READY || this->phase_ == Phase::SEARCH ||
         this->phase_ == Phase::COMPLEMENT
#ifdef USE_ONE_WIRE_DEVICE_THERMOMETER
         || this->phase_ == Phase::READ_PAD
#endif
#ifdef USE_ONE_WIRE_DEVICE_DS2413
         || this->phase_ == Phase::PIO_READ || this->phase_ == Phase::PIO_REPLY
#endif
      ;
}
#ifdef USE_ONE_WIRE_DEVICE_DS2413
uint8_t IRAM_ATTR DeviceModel::pio_status_(uint8_t inputs) const {
  uint8_t pins = inputs & this->pio_latch_;
  uint8_t nibble = (pins & 1) | ((this->pio_latch_ & 1) << 1) | ((pins & 2) << 1) | ((this->pio_latch_ & 2) << 2);
  return nibble | ((~nibble & 0x0f) << 4);
}
#endif
bool IRAM_ATTR DeviceModel::transmit_bit([[maybe_unused]] uint8_t inputs) {
  if (this->phase_ == Phase::READY)
    return true;
  if (this->phase_ == Phase::SEARCH || this->phase_ == Phase::COMPLEMENT) {
    const bool value = (this->rom_[this->position_ / 8] >> (this->position_ % 8)) & 1;
    bool complement = this->phase_ == Phase::COMPLEMENT;
    this->phase_ = complement ? Phase::BRANCH : Phase::COMPLEMENT;
    return value != complement;
  }
  if (!this->is_transmitting())
    return true;
  uint8_t value = 0;
  if (this->phase_ == Phase::READ_ROM)
    value = this->rom_[this->position_ / 8];
#ifdef USE_ONE_WIRE_DEVICE_DS2413
  if (this->phase_ == Phase::PIO_READ) {
    if (this->position_ == 0)
      this->pio_reply_[0] = this->pio_status_(inputs);
    value = this->pio_reply_[0];
  } else if (this->phase_ == Phase::PIO_REPLY) {
    value = this->pio_reply_[this->position_ / 8];
  }
#endif
#ifdef USE_ONE_WIRE_DEVICE_THERMOMETER
  if (this->phase_ == Phase::READ_PAD)
    value = this->pad_[this->position_ / 8];
#endif
  bool bit = (value >> (this->position_ % 8)) & 1;
  this->position_++;
#ifdef USE_ONE_WIRE_DEVICE_DS2413
  if (this->phase_ == Phase::PIO_READ)
    this->position_ %= 8;
  if (this->phase_ == Phase::PIO_REPLY && this->position_ == 16) {
    this->phase_ = Phase::PIO_WRITE;
    this->position_ = this->write_position_ = 0;
  }
#endif
  if (this->phase_ == Phase::READ_ROM && this->position_ == 64) {
    this->phase_ = this->family_ == FAMILY_DS2401 ? Phase::IGNORE : Phase::FUNCTION;
  }
#ifdef USE_ONE_WIRE_DEVICE_THERMOMETER
  if (this->phase_ == Phase::READ_PAD && this->position_ == 72)
    this->phase_ = Phase::IGNORE;
#endif
  return bit;
}
void IRAM_ATTR DeviceModel::receive_bit(bool bit, int16_t temperature, uint8_t inputs) {
  if (this->phase_ == Phase::MATCH || this->phase_ == Phase::BRANCH) {
    bool expected = (this->rom_[this->position_ / 8] >> (this->position_ % 8)) & 1;
    if (bit != expected) {
      this->phase_ = Phase::IGNORE;
    } else if (++this->position_ == 64) {
      this->phase_ = this->phase_ == Phase::MATCH ? Phase::FUNCTION : Phase::IGNORE;
    } else if (this->phase_ == Phase::BRANCH) {
      this->phase_ = Phase::SEARCH;
    }
    return;
  }
  if (this->phase_ != Phase::ROM && this->phase_ != Phase::FUNCTION && this->phase_ != Phase::WRITE_PAD &&
      this->phase_ != Phase::PIO_WRITE)
    return;
  this->rx_ |= static_cast<uint8_t>(bit) << this->rx_bits_;
  if (++this->rx_bits_ != 8)
    return;
  uint8_t byte = this->rx_;
  this->rx_ = this->rx_bits_ = 0;
  this->command_(byte, temperature, inputs);
}
void IRAM_ATTR DeviceModel::command_(uint8_t command, [[maybe_unused]] int16_t temperature,
                                     [[maybe_unused]] uint8_t inputs) {
#ifdef USE_ONE_WIRE_DEVICE_THERMOMETER
  if (this->phase_ == Phase::WRITE_PAD) {
    this->pending_[this->write_position_++] = command;
    if (this->write_position_ == (this->is_ds18s20_() ? 2 : 3)) {
      this->pad_[2] = this->pending_[0];
      this->pad_[3] = this->pending_[1];
      if (!this->is_ds18s20_())
        this->pad_[4] = (this->pending_[2] & 0x60) | 0x1f;
      this->update_crc_();
      this->phase_ = Phase::IGNORE;
    }
    return;
  }
#endif
#ifdef USE_ONE_WIRE_DEVICE_DS2413
  if (this->phase_ == Phase::PIO_WRITE) {
    if (this->write_position_++ == 0) {
      this->pending_[0] = command;
      return;
    }
    if (static_cast<uint8_t>(~command) != this->pending_[0] || (this->pending_[0] & 0xfc) != 0xfc) {
      this->phase_ = Phase::READY;
      return;
    }
    this->pio_latch_ = this->pending_[0] & 3;
    this->pio_reply_[0] = 0xaa;
    this->pio_reply_[1] = this->pio_status_(inputs);
    this->position_ = 0;
    this->phase_ = Phase::PIO_REPLY;
    return;
  }
#endif
  if (this->phase_ == Phase::ROM) {
    this->position_ = 0;
    if (command == 0x33 || (this->family_ == FAMILY_DS2401 && command == 0x0f)) {
      this->phase_ = Phase::READ_ROM;
    } else if (command == 0xf0 || (command == 0xec && this->has_alarm_())) {
      this->phase_ = Phase::SEARCH;
    } else if (command == 0x55 && this->family_ != FAMILY_DS2401) {
      this->phase_ = Phase::MATCH;
    } else if (command == 0xcc && this->family_ != FAMILY_DS2401) {
      this->phase_ = Phase::FUNCTION;
    } else {
      this->phase_ = Phase::IGNORE;
    }
    return;
  }
  this->position_ = this->write_position_ = 0;
  this->phase_ = Phase::IGNORE;
#ifdef USE_ONE_WIRE_DEVICE_DS2413
  if (this->family_ == FAMILY_DS2413) {
    if (command == 0xf5)
      this->phase_ = Phase::PIO_READ;
    if (command == 0x5a)
      this->phase_ = Phase::PIO_WRITE;
    return;
  }
#endif
#ifdef USE_ONE_WIRE_DEVICE_THERMOMETER
  if (!this->is_thermometer())
    return;
  if (command == 0xbe) {
    {
      this->phase_ = Phase::READ_PAD;
    }
  } else if (command == 0x44) {
    int16_t raw = temperature;
    if (this->is_ds18s20_()) {
      raw = (temperature + 4) >> 3;
      this->pad_[6] = 12 + (raw >> 1) * 16 - temperature;
    } else {
      raw = static_cast<uint16_t>(raw) & static_cast<uint16_t>(0xffffU << (3 - ((this->pad_[4] >> 5) & 3)));
      this->pad_[6] = 0;  // Distinguish a valid 85 C conversion from the power-on sentinel.
    }
    this->pad_[0] = static_cast<uint8_t>(raw);
    this->pad_[1] = static_cast<uint8_t>(raw >> 8);
    int8_t whole = static_cast<int8_t>(this->is_ds18s20_() ? raw >> 1 : raw >> 4);
    this->alarm_ = whole >= static_cast<int8_t>(this->pad_[2]) || whole <= static_cast<int8_t>(this->pad_[3]);
    this->update_crc_();
    this->phase_ = Phase::READY;
  } else if (command == 0x4e) {
    {
      this->phase_ = Phase::WRITE_PAD;
    }
  } else if (command == 0x48 || command == 0xb8) {
    for (uint8_t i = 0; i < 3; i++) {
      if (command == 0x48) {
        this->saved_[i] = this->pad_[i + 2];
      } else {
        this->pad_[i + 2] = this->saved_[i];
      }
    }
    this->update_crc_();
    this->phase_ = Phase::READY;
  } else if (command == 0xb4) {
    {
      this->phase_ = Phase::READY;
    }
  }
#endif
}
#ifdef USE_ONE_WIRE_DEVICE_THERMOMETER
void IRAM_ATTR DeviceModel::update_crc_() {
  uint8_t crc = 0;
  for (uint8_t i = 0; i < 8; i++) {
    uint8_t value = this->pad_[i];
    for (uint8_t b = 0; b < 8; b++) {
      bool mix = (crc ^ value) & 1;
      crc >>= 1;
      if (mix)
        crc ^= 0x8c;
      value >>= 1;
    }
  }
  this->pad_[8] = crc;
}
#endif
}  // namespace esphome::one_wire_device
