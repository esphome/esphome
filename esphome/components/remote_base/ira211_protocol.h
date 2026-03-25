#pragma once

#include "remote_base.h"
#include <algorithm>
#include <array>
#include <vector>

namespace esphome {
namespace remote_base {

// Commands (decoded values)
enum class IRA211Command : uint8_t {
  TEMP_UP = 1,
  TEMP_DOWN = 2,
  MODE = 4,
  FAN = 5,
  POWER = 6,
  SYNC = 7,
};

// Modes (decoded values — full-byte rev8(~wire))
enum class IRA211Mode : uint8_t {
  PROTECTION = 0,
  TIMER = 85,
  COMFORT = 170,
};

// Fan speeds (decoded values — full-byte rev8(~wire))
enum class IRA211Fan : uint8_t {
  FAN_AUTO = 3,
  FAN_LOW = 12,
  FAN_MEDIUM = 48,
  FAN_HIGH = 192,
};

// Device ID (wire byte)
static constexpr uint8_t IRA211_DEVICE_ID = 0x66;

// Frame limits
static constexpr uint8_t IRA211_MAX_PACKETS = 7;
static constexpr uint8_t IRA211_MAX_FRAME_BITS = IRA211_MAX_PACKETS * 10;           // 70
static constexpr uint8_t IRA211_MAX_FRAME_BYTES = (IRA211_MAX_FRAME_BITS + 7) / 8;  // 9

/*
  IRA211 IR Protocol — Siemens IRA211 thermostat remote

  NRZ run-length encoding, 38 kHz carrier, T = 800 µs base unit.
  Preamble: 7600 µs mark, 800 µs space, 800 µs mark, 7600 µs space.

  Data is structured as 10-bit packets: 1 (start) + 8 data bits + 0 (end).
  Frame sizes: 4 packets (40b) for mode/fan, 5 packets (50b) for temp,
               7 packets (70b) for sync/power.

  All field bytes encode/decode via the same self-inverse transform:
    wire_byte = rev8(~value)
    value     = rev8(~wire_byte)

  Last packet is always a checksum:
    checksum = rev8((N + Σrev8(b)) & 0xFF)
  where N = number of data packets, rev8 = pure bit reversal, b = wire bytes.
*/

class IRA211Data {
 public:
  // Default constructor — empty frame, use setters to build
  IRA211Data() {}

  // Construct from incoming NRZ bitstream (packed MSB-first, sans preamble)
  IRA211Data(const uint8_t *bitstream, uint8_t num_bits) {
    const uint8_t clamped_bits = std::min(num_bits, static_cast<uint8_t>(IRA211_MAX_FRAME_BITS));
    this->frame_bits_ = clamped_bits;
    uint8_t num_bytes = (clamped_bits + 7) / 8;
    std::copy_n(bitstream, std::min(num_bytes, static_cast<uint8_t>(this->data_.size())), this->data_.begin());
    this->parse_();
  }

  // Construct from vector
  IRA211Data(const std::vector<uint8_t> &bitstream, uint8_t num_bits) : IRA211Data(bitstream.data(), num_bits) {}

  // Getters — return decoded backing fields
  IRA211Command get_command() const { return this->command_; }
  uint8_t get_temperature() const { return this->temperature_; }
  uint8_t get_temp_tenths() const { return this->temp_tenths_; }
  IRA211Mode get_mode() const { return this->mode_; }
  IRA211Fan get_fan() const { return this->fan_; }

  // Frame data access for NRZ encoding
  const uint8_t *get_frame_data() const { return this->data_.data(); }
  uint8_t get_frame_bits() const { return this->frame_bits_; }

  // Public static helpers for decode — allows determining frame length from first 20 bits
  static uint8_t wire_decode_public(uint8_t wire) { return wire_decode(wire); }
  static uint8_t packet_count_public(IRA211Command cmd) { return packet_count(cmd); }

  // Setters — update backing field, then rebuild the data array
  void set_command(IRA211Command cmd) {
    this->command_ = cmd;
    this->rebuild_();
  }
  void set_command(uint8_t cmd) { this->set_command(static_cast<IRA211Command>(cmd)); }
  void set_temperature(uint8_t temp, uint8_t tenths) {
    this->temperature_ = temp;
    this->temp_tenths_ = tenths;
    this->rebuild_();
  }
  void set_mode(IRA211Mode mode) {
    this->mode_ = mode;
    this->rebuild_();
  }
  void set_mode(uint8_t mode) { this->set_mode(static_cast<IRA211Mode>(mode)); }
  void set_fan(IRA211Fan fan) {
    this->fan_ = fan;
    this->rebuild_();
  }
  void set_fan(uint8_t fan) { this->set_fan(static_cast<IRA211Fan>(fan)); }

  // Compute checksum and write into last packet — call before transmit
  void finalize() {
    if (this->frame_bits_ == 0 || this->frame_bits_ % 10 != 0)
      return;
    if (this->frame_bits_ > IRA211_MAX_FRAME_BITS)
      return;
    uint8_t num_pkts = this->frame_bits_ / 10;
    if (num_pkts < 4 || num_pkts > IRA211_MAX_PACKETS)
      return;
    this->write_packet_(num_pkts - 1, this->compute_checksum_());
  }

  // Validate device ID, boundary bits, and checksum
  bool is_valid() const {
    if (this->frame_bits_ == 0 || this->frame_bits_ % 10 != 0)
      return false;
    uint8_t num_pkts = this->frame_bits_ / 10;
    if (num_pkts < 4 || num_pkts > IRA211_MAX_PACKETS)
      return false;
    // Check device ID
    if (this->get_packet_byte_(0) != IRA211_DEVICE_ID)
      return false;
    // Check boundary bits: each packet starts with 1, ends with 0
    for (uint8_t p = 0; p < num_pkts; p++) {
      if (!this->get_bit_(p * 10))
        return false;
      if (this->get_bit_(p * 10 + 9))
        return false;
    }
    // Check checksum
    if (this->get_packet_byte_(num_pkts - 1) != this->compute_checksum_())
      return false;
    // Verify frame length matches command's expected packet count
    auto cmd = static_cast<IRA211Command>(wire_decode(this->get_packet_byte_(1)));
    return packet_count(cmd) == num_pkts;
  }

  bool operator==(const IRA211Data &rhs) const {
    if (this->frame_bits_ != rhs.frame_bits_)
      return false;
    uint8_t num_bytes = (this->frame_bits_ + 7) / 8;
    return std::equal(this->data_.begin(), this->data_.begin() + num_bytes, rhs.data_.begin());
  }

 protected:
  // Packed bitstream (MSB-first), includes 10-bit boundary bits
  std::array<uint8_t, IRA211_MAX_FRAME_BYTES> data_{};
  uint8_t frame_bits_{0};

  // Decoded backing fields
  IRA211Command command_{IRA211Command::SYNC};
  uint8_t temperature_{20};
  uint8_t temp_tenths_{0};
  IRA211Mode mode_{IRA211Mode::COMFORT};
  IRA211Fan fan_{IRA211Fan::FAN_AUTO};

  // Pure 8-bit reversal (no inversion)
  static uint8_t rev8(uint8_t byte) {
    byte = ((byte & 0xF0) >> 4) | ((byte & 0x0F) << 4);
    byte = ((byte & 0xCC) >> 2) | ((byte & 0x33) << 2);
    byte = ((byte & 0xAA) >> 1) | ((byte & 0x55) << 1);
    return byte;
  }

  // Wire encode/decode: rev8(~value), self-inverse
  static uint8_t wire_encode(uint8_t value) { return rev8(~value); }
  static uint8_t wire_decode(uint8_t wire) { return rev8(~wire); }

  // Encode enum types to wire format
  static uint8_t wire_encode(IRA211Command v) { return wire_encode(static_cast<uint8_t>(v)); }
  static uint8_t wire_encode(IRA211Mode v) { return wire_encode(static_cast<uint8_t>(v)); }
  static uint8_t wire_encode(IRA211Fan v) { return wire_encode(static_cast<uint8_t>(v)); }

  // Bit-level access on packed data_ array (MSB-first packing)
  bool get_bit_(uint8_t n) const { return (this->data_[n >> 3] >> (7 - (n & 7))) & 1; }
  void set_bit_(uint8_t n, bool v) {
    uint8_t &byte = this->data_[n >> 3];
    uint8_t mask = 1 << (7 - (n & 7));
    if (v) {
      byte |= mask;
    } else {
      byte &= ~mask;
    }
  }

  // Extract inner 8-bit byte from 10-bit packet at index p (bits p*10+1..p*10+8)
  uint8_t get_packet_byte_(uint8_t p) const {
    uint8_t offset = p * 10 + 1;  // skip start boundary bit
    uint8_t result = 0;
    for (uint8_t i = 0; i < 8; i++) {
      result = (result << 1) | this->get_bit_(offset + i);
    }
    return result;
  }

  // Write boundary bits + inner byte into 10-bit packet at index p
  void write_packet_(uint8_t p, uint8_t data_byte) {
    uint8_t offset = p * 10;
    this->set_bit_(offset, true);  // start boundary = 1
    for (uint8_t i = 0; i < 8; i++) {
      this->set_bit_(offset + 1 + i, (data_byte >> (7 - i)) & 1);
    }
    this->set_bit_(offset + 9, false);  // end boundary = 0
  }

  // Number of packets for a given command
  static uint8_t packet_count(IRA211Command cmd) {
    switch (cmd) {
      case IRA211Command::SYNC:
      case IRA211Command::POWER:
        return 7;
      case IRA211Command::TEMP_UP:
      case IRA211Command::TEMP_DOWN:
        return 5;
      case IRA211Command::MODE:
      case IRA211Command::FAN:
        return 4;
      default:
        return 0;
    }
  }

  // Checksum: rev8((N + Σrev8(wire_byte)) & 0xFF) where rev8 is pure bit reversal
  uint8_t compute_checksum_() const {
    uint8_t num_pkts = this->frame_bits_ / 10;
    uint8_t n = num_pkts - 1;  // data packets (excluding checksum)
    uint16_t sum = n;
    for (uint8_t i = 0; i < n; i++) {
      sum += rev8(this->get_packet_byte_(i));
    }
    return rev8(static_cast<uint8_t>(sum & 0xFF));
  }

  // Reconstruct data_ array from backing fields (does NOT write checksum — call finalize())
  void rebuild_() {
    this->data_.fill(0);
    uint8_t num_pkts = packet_count(this->command_);
    this->frame_bits_ = num_pkts * 10;
    if (num_pkts == 0)
      return;

    this->write_packet_(0, IRA211_DEVICE_ID);
    this->write_packet_(1, wire_encode(this->command_));

    switch (this->command_) {
      case IRA211Command::SYNC:
      case IRA211Command::POWER:
        this->write_packet_(2, wire_encode(this->temperature_));
        this->write_packet_(3, wire_encode(this->temp_tenths_));
        this->write_packet_(4, wire_encode(this->mode_));
        this->write_packet_(5, wire_encode(this->fan_));
        break;
      case IRA211Command::TEMP_UP:
      case IRA211Command::TEMP_DOWN:
        this->write_packet_(2, wire_encode(this->temperature_));
        this->write_packet_(3, wire_encode(this->temp_tenths_));
        break;
      case IRA211Command::MODE:
        this->write_packet_(2, wire_encode(this->mode_));
        break;
      case IRA211Command::FAN:
        this->write_packet_(2, wire_encode(this->fan_));
        break;
    }
  }

  // Extract backing fields from data_ array
  void parse_() {
    uint8_t num_pkts = this->frame_bits_ / 10;
    if (num_pkts < 4)
      return;

    this->command_ = static_cast<IRA211Command>(wire_decode(this->get_packet_byte_(1)));

    switch (this->command_) {
      case IRA211Command::SYNC:
      case IRA211Command::POWER:
        this->temperature_ = wire_decode(this->get_packet_byte_(2));
        this->temp_tenths_ = wire_decode(this->get_packet_byte_(3));
        this->mode_ = static_cast<IRA211Mode>(wire_decode(this->get_packet_byte_(4)));
        this->fan_ = static_cast<IRA211Fan>(wire_decode(this->get_packet_byte_(5)));
        break;
      case IRA211Command::TEMP_UP:
      case IRA211Command::TEMP_DOWN:
        this->temperature_ = wire_decode(this->get_packet_byte_(2));
        this->temp_tenths_ = wire_decode(this->get_packet_byte_(3));
        break;
      case IRA211Command::MODE:
        this->mode_ = static_cast<IRA211Mode>(wire_decode(this->get_packet_byte_(2)));
        break;
      case IRA211Command::FAN:
        this->fan_ = static_cast<IRA211Fan>(wire_decode(this->get_packet_byte_(2)));
        break;
    }
  }
};

class IRA211Protocol : public RemoteProtocol<IRA211Data> {
 public:
  void encode(RemoteTransmitData *dst, const IRA211Data &data) override;
  optional<IRA211Data> decode(RemoteReceiveData src) override;
  void dump(const IRA211Data &data) override;
};

class IRA211BinarySensor : public RemoteReceiverBinarySensorBase {
 public:
  bool matches(RemoteReceiveData src) override {
    auto data = IRA211Protocol().decode(src);
    return data.has_value() && data.value() == this->data_;
  }
  void set_command(IRA211Command cmd) { this->data_.set_command(cmd); }
  void set_command(uint8_t cmd) { this->data_.set_command(cmd); }
  void set_temperature(uint8_t temp, uint8_t tenths) { this->data_.set_temperature(temp, tenths); }
  void set_mode(IRA211Mode mode) { this->data_.set_mode(mode); }
  void set_mode(uint8_t mode) { this->data_.set_mode(mode); }
  void set_fan(IRA211Fan fan) { this->data_.set_fan(fan); }
  void set_fan(uint8_t fan) { this->data_.set_fan(fan); }
  void finalize() { this->data_.finalize(); }

 protected:
  IRA211Data data_;
};

using IRA211Trigger = RemoteReceiverTrigger<IRA211Protocol>;
using IRA211Dumper = RemoteReceiverDumper<IRA211Protocol>;

template<typename... Ts> class IRA211Action : public RemoteTransmitterActionBase<Ts...> {
  TEMPLATABLE_VALUE(IRA211Command, command)
  TEMPLATABLE_VALUE(uint8_t, temperature)
  TEMPLATABLE_VALUE(uint8_t, temp_tenths)
  TEMPLATABLE_VALUE(IRA211Mode, mode)
  TEMPLATABLE_VALUE(IRA211Fan, fan)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    IRA211Data data;
    data.set_command(this->command_.value(x...));
    data.set_temperature(this->temperature_.value_or(x..., 20), this->temp_tenths_.value_or(x..., 0));
    data.set_mode(this->mode_.value_or(x..., IRA211Mode::COMFORT));
    data.set_fan(this->fan_.value_or(x..., IRA211Fan::FAN_AUTO));
    data.finalize();
    IRA211Protocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
