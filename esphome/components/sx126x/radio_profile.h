#pragma once
#include "sx126x_reg.h"
#include <array>
#include <span>
#include <algorithm>

namespace esphome::sx126x {
enum SX126xBw : uint8_t {
  // FSK
  SX126X_BW_4800,
  SX126X_BW_5800,
  SX126X_BW_7300,
  SX126X_BW_9700,
  SX126X_BW_11700,
  SX126X_BW_14600,
  SX126X_BW_19500,
  SX126X_BW_23400,
  SX126X_BW_29300,
  SX126X_BW_39000,
  SX126X_BW_46900,
  SX126X_BW_58600,
  SX126X_BW_78200,
  SX126X_BW_93800,
  SX126X_BW_117300,
  SX126X_BW_156200,
  SX126X_BW_187200,
  SX126X_BW_234300,
  SX126X_BW_312000,
  SX126X_BW_373600,
  SX126X_BW_467000,
  // LORA
  SX126X_BW_7810,
  SX126X_BW_10420,
  SX126X_BW_15630,
  SX126X_BW_20830,
  SX126X_BW_31250,
  SX126X_BW_41670,
  SX126X_BW_62500,
  SX126X_BW_125000,
  SX126X_BW_250000,
  SX126X_BW_500000,
};

// Requested settings are separate from the last successfully applied profile.
// Bounded sync storage makes profile snapshots allocation-free.
struct RadioProfile {
  uint32_t bitrate{0};
  uint32_t deviation{0};
  uint32_t frequency{0};
  uint32_t payload_length{0};
  uint32_t tcxo_delay{0};
  uint16_t crc_polynomial{0};
  uint16_t crc_initial{0};
  uint16_t whitening_initial{0};
  uint16_t preamble_detect{0};
  uint16_t preamble_size{0};
  SX126xBw bandwidth{SX126X_BW_125000};
  uint8_t crc_size{0};
  uint8_t tcxo_voltage{0};
  uint8_t coding_rate{0};
  uint8_t modulation{PACKET_TYPE_LORA};
  uint8_t pa_ramp{0};
  uint8_t shaping{0};
  uint8_t spreading_factor{0};
  int8_t pa_power{0};
  bool sx1261{false};
  bool crc_enable{false};
  bool crc_inverted{false};
  bool whitening_enable{false};
  bool rx_start{false};
  bool rf_switch{false};
  std::array<uint8_t, 8> sync_value{};
  uint8_t sync_size{0};
  bool sync_valid{true};
  void set_sync_value(std::span<const uint8_t> bytes) {
    this->sync_valid = bytes.size() <= this->sync_value.size();
    if (!this->sync_valid) {
      return;
    }
    std::copy(bytes.begin(), bytes.end(), this->sync_value.begin());
    this->sync_size = bytes.size();
  }
  size_t max_packet_size() const { return this->payload_length > 0 ? this->payload_length : 255; }
};
}  // namespace esphome::sx126x
