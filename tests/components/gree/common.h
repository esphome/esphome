#pragma once

#include <gtest/gtest.h>

#include "esphome/components/gree/gree.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome::gree::testing {

using remote_base::RawTimings;

inline remote_base::RemoteReceiveData receive_data(const RawTimings &raw) {
  return remote_base::RemoteReceiveData(raw, 25, remote_base::TOLERANCE_MODE_PERCENTAGE);
}

inline RawTimings encode_signal(Model model, const GreeState &state) {
  remote_base::RemoteTransmitData data;
  GreeProtocol(model).encode(&data, state);
  return data.get_data();
}

inline optional<GreeState> decode_signal(Model model, const RawTimings &raw) {
  return GreeProtocol(model).decode(receive_data(raw));
}

inline GreeState with_checksum(GreeState state) {
  state[GREE_STATE_FRAME_SIZE - 1] = GreeProtocol::calculate_checksum(state);
  return state;
}

/// Hand raw timings to the climate as the remote receiver would.
inline bool receive_raw(GreeClimate &device, const RawTimings &raw) {
  remote_base::RemoteReceiverListener *listener = &device;
  return listener->on_receive(receive_data(raw));
}

inline bool receive(GreeClimate &device, Model model, const GreeState &state) {
  return receive_raw(device, encode_signal(model, state));
}

/// Counts transmissions and keeps the last one's timings instead of driving IR hardware.
class CapturingTransmitter final : public remote_base::RemoteTransmitterBase {
 public:
  CapturingTransmitter() : RemoteTransmitterBase(nullptr) {}

  /// The last transmitted frame, decoded with GreeProtocol.
  GreeState frame(Model model) const {
    const auto state = decode_signal(model, this->last_raw);
    EXPECT_TRUE(state.has_value());
    return state.value_or(GreeState{});
  }

  uint32_t send_count{0};
  RawTimings last_raw;
  uint32_t last_carrier_frequency{0};

 protected:
  void send_internal(uint32_t send_times, uint32_t /*send_wait*/) override {
    this->send_count += send_times;
    this->last_raw = this->temp_.get_data();
    this->last_carrier_frequency = this->temp_.get_carrier_frequency();
  }
};

}  // namespace esphome::gree::testing
