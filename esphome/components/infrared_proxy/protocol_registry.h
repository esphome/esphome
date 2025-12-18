#pragma once

#include "esphome/core/optional.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/components/json/json_util.h"

#include <string>
#include <vector>

namespace esphome::infrared_proxy {

/// Parse JSON protocol specification and encode to IR/RF timings using remote_base protocols
/// @param protocol_json JSON string containing protocol name and parameters
/// @param transmit_data Output buffer for encoded IR/RF timings
/// @return true if successful, false if invalid protocol or parameters
bool encode_from_json(const std::string &protocol_json, remote_base::RemoteTransmitData *transmit_data);

/// Write JSON-formatted list of all supported infrared/RF protocols directly to output string
/// @param out Output string to write JSON array to (avoids intermediate allocations)
void write_supported_protocols_json(std::string &out);

}  // namespace esphome::infrared_proxy
