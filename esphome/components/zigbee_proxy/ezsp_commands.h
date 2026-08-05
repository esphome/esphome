#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome::zigbee_proxy {

// EZSP Protocol Versions
static constexpr uint8_t EZSP_MIN_VERSION = 13;  // Minimum supported version
static constexpr uint8_t EZSP_MAX_VERSION = 13;  // Maximum version we request

// EZSP Frame Control bits
static constexpr uint8_t EZSP_FRAME_CONTROL_COMMAND = 0x00;   // Host to NCP
static constexpr uint8_t EZSP_FRAME_CONTROL_RESPONSE = 0x80;  // NCP to Host
static constexpr uint8_t EZSP_FRAME_CONTROL_CALLBACK = 0x90;  // Async callback from NCP

// High byte of the 16-bit frame control, carrying frameFormatVersion = 1. Every
// command after version negotiation must set this: omitting it leaves the NCP
// reading the frame ID's low byte as frame_control_high, so the command is
// discarded and the reply is an error frame rather than the expected response.
static constexpr uint8_t EZSP_FRAME_CONTROL_EXTENDED = 0x01;

// Legacy EZSP frame format (v4-v7): [sequence] [frame_control] [frame_id]
// Extended EZSP frame format (v8+): [sequence] [frame_control_low] [frame_control_high] [frame_id_low] [frame_id_high]
//
// Only the `version` command and its response use the legacy format, because the
// NCP starts in legacy mode and has not yet learned the negotiated version.
// Everything after that is extended, with no per-NCP exceptions.

// EZSP Frame IDs - Commands (host to NCP)
static constexpr uint16_t EZSP_VERSION = 0x0000;                 // Version negotiation
static constexpr uint16_t EZSP_NETWORK_INIT = 0x0017;            // Initialize network
static constexpr uint16_t EZSP_NETWORK_STATE = 0x0018;           // Get network state
static constexpr uint16_t EZSP_GET_EUI64 = 0x0026;               // Get IEEE address
static constexpr uint16_t EZSP_GET_NETWORK_PARAMETERS = 0x0028;  // Get network parameters
static constexpr uint16_t EZSP_SET_CONFIGURATION_VALUE = 0x0053;  // Set a stack config value

// Stack configuration. CONFIG_STACK_PROFILE must be set to ZigBee PRO before
// networkInit, or the NCP answers NOT_JOINED for a node that is in fact joined
// and getNetworkParameters then fails, leaving PAN ID and channel unreadable.
// The NCP boots with stack profile 0, so this is not optional.
static constexpr uint8_t EZSP_CONFIG_STACK_PROFILE = 0x0C;
static constexpr uint16_t STACK_PROFILE_ZIGBEE_PRO = 2;

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

// Status codes (subset). EZSP v13+ / EmberZNet 8.x report sl_status_t, not the
// legacy 8-bit EmberStatus, and the two disagree on every value that matters
// here: legacy NETWORK_UP/NOT_JOINED were 0x90/0x93.
enum class SlStatus : uint8_t {
  OK = 0x00,
  NETWORK_UP = 0x15,
  NETWORK_DOWN = 0x16,
  NOT_JOINED = 0x17,
};

// sl_status_t is 32-bit little-endian on the wire, so a status field occupies
// four bytes even though every code used here fits in the first one.
static constexpr size_t SL_STATUS_SIZE = 4;

// Network parameters structure offsets (in getNetworkParameters response)
// Response format: [status (4)] [nodeType (1)] [parameters (20)] = 25 bytes
// Parameters: [extendedPanId (8)] [panId (2)] [radioTxPower (1)] [radioChannel (1)]
//             [joinMethod (1)] [nwkManagerId (2)] [nwkUpdateId (1)] [channels (4)]
static constexpr size_t NETWORK_PARAMS_STATUS_OFFSET = 0;
static constexpr size_t NETWORK_PARAMS_NODE_TYPE_OFFSET = 4;
static constexpr size_t NETWORK_PARAMS_EXT_PAN_ID_OFFSET = 5;
static constexpr size_t NETWORK_PARAMS_PAN_ID_OFFSET = 13;
static constexpr size_t NETWORK_PARAMS_CHANNEL_OFFSET = 16;
static constexpr size_t NETWORK_PARAMS_RESPONSE_SIZE = 25;

}  // namespace esphome::zigbee_proxy
