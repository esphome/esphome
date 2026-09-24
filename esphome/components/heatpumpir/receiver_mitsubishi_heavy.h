#pragma once

#include "esphome/components/remote_base/remote_base.h"

namespace esphome::heatpumpir {

class HeatpumpIRClimate;

/// Length of a Mitsubishi Heavy IR frame (header + payload).
static constexpr uint8_t FRAME_LENGTH = 11;

/// Read and validate a Mitsubishi Heavy IR frame (header, 11 bytes, XOR checksums).
/// Decodes shared fields: mode and target_temperature. Fills frame[FRAME_LENGTH] with raw bytes.
/// Returns true if a valid frame was read and decoded.
bool decode_mitsubishi_heavy_frame(remote_base::RemoteReceiveData &data, uint8_t frame[FRAME_LENGTH],
                                   HeatpumpIRClimate &climate);

/// Decode ZMP-specific frame bytes (fan, preset, swing) into `climate`.
bool decode_mitsubishi_heavy_zmp(const uint8_t frame[FRAME_LENGTH], HeatpumpIRClimate &climate);

}  // namespace esphome::heatpumpir
