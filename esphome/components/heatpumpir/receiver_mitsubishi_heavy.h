#pragma once

#include "esphome/components/remote_base/remote_base.h"

namespace esphome::heatpumpir {

class HeatpumpIRClimate;

/// Bytes in a Mitsubishi Heavy IR frame, prefix included
inline constexpr uint8_t MITSUBISHI_HEAVY_FRAME_LENGTH = 11;

/// Read a Mitsubishi Heavy frame into `frame`, check its prefix and inverted byte pairs, and apply the
/// fields every variant shares (mode and target temperature). Returns false when no valid frame was read.
bool decode_mitsubishi_heavy_frame(remote_base::RemoteReceiveData &data, uint8_t frame[MITSUBISHI_HEAVY_FRAME_LENGTH],
                                   HeatpumpIRClimate &climate);

/// Apply the ZMP-specific fields (fan, preset, swing) of a validated frame.
void decode_mitsubishi_heavy_zmp(const uint8_t frame[MITSUBISHI_HEAVY_FRAME_LENGTH], HeatpumpIRClimate &climate);

}  // namespace esphome::heatpumpir
