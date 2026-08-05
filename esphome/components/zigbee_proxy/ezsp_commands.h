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
static constexpr uint16_t EZSP_VERSION = 0x0000;         // Version negotiation
static constexpr uint16_t EZSP_GET_EUI64 = 0x0026;       // Get IEEE address
static constexpr uint16_t EZSP_GET_TOKEN_DATA = 0x0102;  // Read an NVM3 token

// Network metadata comes straight out of NVM3 instead of from a running stack.
// NVM3KEY_STACK_NODE_DATA holds the PAN ID, channel, extended PAN ID and node type of
// the network this radio is commissioned onto, and reading it requires nothing beyond a
// completed version negotiation: no stack configuration, no networkInit, no waiting on
// stackStatusHandler, and above all no joining the network -- so simply plugging the
// device in never brings the radio up.
//
// Note the 0x0001 domain prefix on the NVM3 object key. The bare creator ID
// 0x0000EE64 is a different thing and getTokenData answers FAIL for it.
static constexpr uint32_t NVM3KEY_STACK_NODE_DATA = 0x0001EE64;

// getTokenData response: [status (4)] [length (4)] [value (length)]
static constexpr size_t TOKEN_DATA_LENGTH_OFFSET = 4;
static constexpr size_t TOKEN_DATA_VALUE_OFFSET = 8;

// NV3StackNodeData value layout (16 bytes, little-endian):
// [panId (2)] [radioTxPower (1)] [radioFreqChannel (1)] [stackProfile (1)]
// [nodeType (1)] [zigbeeNodeId (2)] [extendedPanId (8)]
static constexpr size_t NV3_NODE_DATA_SIZE = 16;
static constexpr size_t NV3_NODE_DATA_PAN_ID_OFFSET = 0;
static constexpr size_t NV3_NODE_DATA_CHANNEL_OFFSET = 3;
static constexpr size_t NV3_NODE_DATA_NODE_TYPE_OFFSET = 5;
static constexpr size_t NV3_NODE_DATA_EXT_PAN_ID_OFFSET = 8;

// A radio with no network still has the token, holding a sentinel rather than being
// absent: panId reads 0xFFFF and nodeType reads UNKNOWN_DEVICE. Detecting "no network"
// therefore means inspecting nodeType, not treating the read as failed.
static constexpr uint8_t NV3_NODE_TYPE_UNKNOWN_DEVICE = 0x00;

// Status codes (subset). EZSP v13+ / EmberZNet 8.x report sl_status_t, not the
// legacy 8-bit EmberStatus.
enum class SlStatus : uint8_t {
  OK = 0x00,
};

// sl_status_t is 32-bit little-endian on the wire, so a status field occupies
// four bytes even though every code used here fits in the first one.
static constexpr size_t SL_STATUS_SIZE = 4;

}  // namespace esphome::zigbee_proxy
