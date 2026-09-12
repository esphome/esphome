#pragma once

#include "esphome/components/remote_base/remote_base.h"

namespace esphome::heatpumpir {

class HeatpumpIRClimate;

/// Read and validate a Mitsubishi Heavy IR frame (header, 11 bytes, XOR checksums).
/// Decodes shared fields: mode and target_temperature. Fills frame[11] with raw bytes.
/// Returns true if a valid frame was read and decoded.
bool decode_mitsubishi_heavy_frame(remote_base::RemoteReceiveData &data, uint8_t frame[11], HeatpumpIRClimate &climate);

/// Decode ZMP-specific frame bytes (fan, preset, swing) into `climate`.
bool decode_mitsubishi_heavy_zmp(const uint8_t frame[11], HeatpumpIRClimate &climate);

}  // namespace esphome::heatpumpir
