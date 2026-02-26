#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome::zigbee_proxy {

// EZSP Protocol Versions
static constexpr uint8_t EZSP_MIN_VERSION = 8;   // Minimum supported version
static constexpr uint8_t EZSP_MAX_VERSION = 13;  // Maximum version we request

// EZSP Frame Control bits
static constexpr uint8_t EZSP_FRAME_CONTROL_COMMAND = 0x00;   // Host to NCP
static constexpr uint8_t EZSP_FRAME_CONTROL_RESPONSE = 0x80;  // NCP to Host
static constexpr uint8_t EZSP_FRAME_CONTROL_CALLBACK = 0x90;  // Async callback from NCP

// Legacy EZSP frame format (v4-v7): [sequence] [frame_control] [frame_id]
// Extended EZSP frame format (v8+): [sequence] [frame_control_low] [frame_control_high] [frame_id_low] [frame_id_high]

// EZSP Frame IDs - Commands (host to NCP)
static constexpr uint16_t EZSP_VERSION = 0x0000;                 // Version negotiation
static constexpr uint16_t EZSP_NETWORK_INIT = 0x0017;            // Initialize network
static constexpr uint16_t EZSP_NETWORK_STATE = 0x0018;           // Get network state
static constexpr uint16_t EZSP_GET_EUI64 = 0x0026;               // Get IEEE address
static constexpr uint16_t EZSP_GET_NETWORK_PARAMETERS = 0x0028;  // Get network parameters

// EZSP Frame IDs - Callbacks (NCP to host, async)
static constexpr uint16_t EZSP_STACK_STATUS_HANDLER = 0x0019;  // Stack status callback

// EZSP Network Status
enum class EzspNetworkStatus : uint8_t {
  NO_NETWORK = 0x00,
  JOINING_NETWORK = 0x01,
  JOINED_NETWORK = 0x02,
  JOINED_NETWORK_NO_PARENT = 0x03,
  LEAVING_NETWORK = 0x04,
};

// Ember Status codes (subset)
enum class EmberStatus : uint8_t {
  SUCCESS = 0x00,
  NETWORK_UP = 0x90,
  NETWORK_DOWN = 0x91,
  NOT_JOINED = 0x93,
};

// Network parameters structure offsets (in getNetworkParameters response)
// Response format: [status] [nodeType] [parameters...]
// Parameters: [extendedPanId (8)] [panId (2)] [radioTxPower] [radioChannel] [joinMethod] ...
static constexpr size_t NETWORK_PARAMS_STATUS_OFFSET = 0;
static constexpr size_t NETWORK_PARAMS_NODE_TYPE_OFFSET = 1;
static constexpr size_t NETWORK_PARAMS_EXT_PAN_ID_OFFSET = 2;
static constexpr size_t NETWORK_PARAMS_PAN_ID_OFFSET = 10;
static constexpr size_t NETWORK_PARAMS_CHANNEL_OFFSET = 13;

}  // namespace esphome::zigbee_proxy
