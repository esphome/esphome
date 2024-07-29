#pragma once

#include "esphome/core/helpers.h"

#include <vector>

namespace esphome {
namespace nfc {

// Header info
static const uint8_t NCI_PKT_HEADER_SIZE = 3;     // NCI packet (pkt) headers are always three bytes
static const uint8_t NCI_PKT_MT_GID_OFFSET = 0;   // NCI packet (pkt) MT and GID offsets
static const uint8_t NCI_PKT_OID_OFFSET = 1;      // NCI packet (pkt) OID offset
static const uint8_t NCI_PKT_LENGTH_OFFSET = 2;   // NCI packet (pkt) message length (size) offset
static const uint8_t NCI_PKT_PAYLOAD_OFFSET = 3;  // NCI packet (pkt) payload offset
// Important masks
static const uint8_t NCI_PKT_MT_MASK = 0xE0;   // NCI packet (pkt) message type mask
static const uint8_t NCI_PKT_PBF_MASK = 0x10;  // packet boundary flag bit
static const uint8_t NCI_PKT_GID_MASK = 0x0F;
static const uint8_t NCI_PKT_OID_MASK = 0x3F;
// Message types
static const uint8_t NCI_PKT_MT_DATA = 0x00;               // For sending commands to NFC endpoint (card/tag)
static const uint8_t NCI_PKT_MT_CTRL_COMMAND = 0x20;       // For sending commands to NFCC
static const uint8_t NCI_PKT_MT_CTRL_RESPONSE = 0x40;      // Response from NFCC to commands
static const uint8_t NCI_PKT_MT_CTRL_NOTIFICATION = 0x60;  // Notification from NFCC
// GIDs
static const uint8_t NCI_CORE_GID = 0x0;
static const uint8_t RF_GID = 0x1;
static const uint8_t NFCEE_GID = 0x1;
static const uint8_t NCI_PROPRIETARY_GID = 0xF;
// OIDs
static const uint8_t NCI_CORE_RESET_OID = 0x00;
static const uint8_t NCI_CORE_INIT_OID = 0x01;
static const uint8_t NCI_CORE_SET_CONFIG_OID = 0x02;
static const uint8_t NCI_CORE_GET_CONFIG_OID = 0x03;
static const uint8_t NCI_CORE_CONN_CREATE_OID = 0x04;
static const uint8_t NCI_CORE_CONN_CLOSE_OID = 0x05;
static const uint8_t NCI_CORE_CONN_CREDITS_OID = 0x06;
static const uint8_t NCI_CORE_GENERIC_ERROR_OID = 0x07;
static const uint8_t NCI_CORE_INTERFACE_ERROR_OID = 0x08;

static const uint8_t RF_DISCOVER_MAP_OID = 0x00;
static const uint8_t RF_SET_LISTEN_MODE_ROUTING_OID = 0x01;
static const uint8_t RF_GET_LISTEN_MODE_ROUTING_OID = 0x02;
static const uint8_t RF_DISCOVER_OID = 0x03;
static const uint8_t RF_DISCOVER_SELECT_OID = 0x04;
static const uint8_t RF_INTF_ACTIVATED_OID = 0x05;
static const uint8_t RF_DEACTIVATE_OID = 0x06;
static const uint8_t RF_FIELD_INFO_OID = 0x07;
static const uint8_t RF_T3T_POLLING_OID = 0x08;
static const uint8_t RF_NFCEE_ACTION_OID = 0x09;
static const uint8_t RF_NFCEE_DISCOVERY_REQ_OID = 0x0A;
static const uint8_t RF_PARAMETER_UPDATE_OID = 0x0B;

static const uint8_t NFCEE_DISCOVER_OID = 0x00;
static const uint8_t NFCEE_MODE_SET_OID = 0x01;
// Interfaces
static const uint8_t INTF_NFCEE_DIRECT = 0x00;
static const uint8_t INTF_FRAME = 0x01;
static const uint8_t INTF_ISODEP = 0x02;
static const uint8_t INTF_NFCDEP = 0x03;
static const uint8_t INTF_TAGCMD = 0x80;  // NXP proprietary
// Bit rates
static const uint8_t NFC_BIT_RATE_106 = 0x00;
static const uint8_t NFC_BIT_RATE_212 = 0x01;
static const uint8_t NFC_BIT_RATE_424 = 0x02;
static const uint8_t NFC_BIT_RATE_848 = 0x03;
static const uint8_t NFC_BIT_RATE_1695 = 0x04;
static const uint8_t NFC_BIT_RATE_3390 = 0x05;
static const uint8_t NFC_BIT_RATE_6780 = 0x06;
// Protocols
static const uint8_t PROT_UNDETERMINED = 0x00;
static const uint8_t PROT_T1T = 0x01;
static const uint8_t PROT_T2T = 0x02;
static const uint8_t PROT_T3T = 0x03;
static const uint8_t PROT_ISODEP = 0x04;
static const uint8_t PROT_NFCDEP = 0x05;
static const uint8_t PROT_T5T = 0x06;
static const uint8_t PROT_MIFARE = 0x80;
// RF Technologies
static const uint8_t NFC_RF_TECH_A = 0x00;
static const uint8_t NFC_RF_TECH_B = 0x01;
static const uint8_t NFC_RF_TECH_F = 0x02;
static const uint8_t NFC_RF_TECH_15693 = 0x03;
// RF Technology & Modes
static const uint8_t MODE_MASK = 0xF0;
static const uint8_t MODE_LISTEN_MASK = 0x80;
static const uint8_t MODE_POLL = 0x00;

static const uint8_t TECH_PASSIVE_NFCA = 0x00;
static const uint8_t TECH_PASSIVE_NFCB = 0x01;
static const uint8_t TECH_PASSIVE_NFCF = 0x02;
static const uint8_t TECH_ACTIVE_NFCA = 0x03;
static const uint8_t TECH_ACTIVE_NFCF = 0x05;
static const uint8_t TECH_PASSIVE_15693 = 0x06;
// Status codes
static const uint8_t STATUS_OK = 0x00;
static const uint8_t STATUS_REJECTED = 0x01;
static const uint8_t STATUS_RF_FRAME_CORRUPTED = 0x02;
static const uint8_t STATUS_FAILED = 0x03;
static const uint8_t STATUS_NOT_INITIALIZED = 0x04;
static const uint8_t STATUS_SYNTAX_ERROR = 0x05;
static const uint8_t STATUS_SEMANTIC_ERROR = 0x06;
static const uint8_t STATUS_INVALID_PARAM = 0x09;
static const uint8_t STATUS_MESSAGE_SIZE_EXCEEDED = 0x0A;
static const uint8_t DISCOVERY_ALREADY_STARTED = 0xA0;
static const uint8_t DISCOVERY_TARGET_ACTIVATION_FAILED = 0xA1;
static const uint8_t DISCOVERY_TEAR_DOWN = 0xA2;
static const uint8_t RF_TRANSMISSION_ERROR = 0xB0;
static const uint8_t RF_PROTOCOL_ERROR = 0xB1;
static const uint8_t RF_TIMEOUT_ERROR = 0xB2;
static const uint8_t NFCEE_INTERFACE_ACTIVATION_FAILED = 0xC0;
static const uint8_t NFCEE_TRANSMISSION_ERROR = 0xC1;
static const uint8_t NFCEE_PROTOCOL_ERROR = 0xC2;
static const uint8_t NFCEE_TIMEOUT_ERROR = 0xC3;
// Deactivation types/reasons
static const uint8_t DEACTIVATION_TYPE_IDLE = 0x00;
static const uint8_t DEACTIVATION_TYPE_SLEEP = 0x01;
static const uint8_t DEACTIVATION_TYPE_SLEEP_AF = 0x02;
static const uint8_t DEACTIVATION_TYPE_DISCOVERY = 0x03;
// RF discover map modes
static const uint8_t RF_DISCOVER_MAP_MODE_POLL = 0x1;
static const uint8_t RF_DISCOVER_MAP_MODE_LISTEN = 0x2;
// RF discover notification types
static const uint8_t RF_DISCOVER_NTF_NT_LAST = 0x00;
static const uint8_t RF_DISCOVER_NTF_NT_LAST_RL = 0x01;
static const uint8_t RF_DISCOVER_NTF_NT_MORE = 0x02;
// Important message offsets
static const uint8_t RF_DISCOVER_NTF_DISCOVERY_ID = 0 + NCI_PKT_HEADER_SIZE;
static const uint8_t RF_DISCOVER_NTF_PROTOCOL = 1 + NCI_PKT_HEADER_SIZE;
static const uint8_t RF_DISCOVER_NTF_MODE_TECH = 2 + NCI_PKT_HEADER_SIZE;
static const uint8_t RF_DISCOVER_NTF_RF_TECH_LENGTH = 3 + NCI_PKT_HEADER_SIZE;
static const uint8_t RF_DISCOVER_NTF_RF_TECH_PARAMS = 4 + NCI_PKT_HEADER_SIZE;
static const uint8_t RF_INTF_ACTIVATED_NTF_DISCOVERY_ID = 0 + NCI_PKT_HEADER_SIZE;
static const uint8_t RF_INTF_ACTIVATED_NTF_INTERFACE = 1 + NCI_PKT_HEADER_SIZE;
static const uint8_t RF_INTF_ACTIVATED_NTF_PROTOCOL = 2 + NCI_PKT_HEADER_SIZE;
static const uint8_t RF_INTF_ACTIVATED_NTF_MODE_TECH = 3 + NCI_PKT_HEADER_SIZE;
static const uint8_t RF_INTF_ACTIVATED_NTF_MAX_SIZE = 4 + NCI_PKT_HEADER_SIZE;
static const uint8_t RF_INTF_ACTIVATED_NTF_INIT_CRED = 5 + NCI_PKT_HEADER_SIZE;
static const uint8_t RF_INTF_ACTIVATED_NTF_RF_TECH_LENGTH = 6 + NCI_PKT_HEADER_SIZE;
static const uint8_t RF_INTF_ACTIVATED_NTF_RF_TECH_PARAMS = 7 + NCI_PKT_HEADER_SIZE;

}  // namespace nfc
}  // namespace esphome
