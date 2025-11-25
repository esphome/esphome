/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include <inttypes.h>

/**
 * @brief USB CDC Descriptor Types
 *
 * @see Table 13, USB CDC specification rev. 1.2
 */
typedef enum
{
    USB_DESCRIPTOR_TYPE_DEVICE = 0x01U,
    USB_DESCRIPTOR_TYPE_CONFIGURATION = 0x02U,
    USB_DESCRIPTOR_TYPE_STRING = 0x03U,
    USB_DESCRIPTOR_TYPE_INTERFACE = 0x04U,
    USB_DESCRIPTOR_TYPE_ENDPOINT = 0x05U,
    USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER = 0x06U,
    USB_DESCRIPTOR_TYPE_OTHER_SPEED = 0x07U,
    USB_DESCRIPTOR_TYPE_INTERFACE_POWER = 0x08U,
    USB_DESCRIPTOR_TYPE_OTG = 0x09U,
    USB_DESCRIPTOR_TYPE_DEBUG = 0x0AU,
    USB_DESCRIPTOR_TYPE_INTERFACE_ASSOCIATION = 0x0BU,
    USB_DESCRIPTOR_TYPE_BINARY_OBJECT_STORE = 0x0FU,
    USB_DESCRIPTOR_TYPE_DEVICE_CAPABILITY = 0x10U,
    USB_DESCRIPTOR_TYPE_WIRELESS_ENDPOINTCOMP = 0x11U,

} __attribute__((packed)) cdc_desc_type_t;

/**
 * @brief USB CDC Descriptor Subtypes
 *
 * @see Table 13, USB CDC specification rev. 1.2
 */
typedef enum
{
    USB_CDC_DESC_SUBTYPE_HEADER = 0x00,             // Header Functional Descriptor
    USB_CDC_DESC_SUBTYPE_CALL = 0x01,               // Call Management Functional Descriptor
    USB_CDC_DESC_SUBTYPE_ACM = 0x02,                // Abstract Control Management Functional Descriptor
    USB_CDC_DESC_SUBTYPE_DLM = 0x03,                // Direct Line Management Functional Descriptor
    USB_CDC_DESC_SUBTYPE_TEL_RINGER = 0x04,         // Telephone Ringer Functional Descriptor
    USB_CDC_DESC_SUBTYPE_TEL_CLSR = 0x05,           // Telephone Call and Line State Reporting Capabilities Functional Descriptor
    USB_CDC_DESC_SUBTYPE_UNION = 0x06,              // Union Functional Descriptor
    USB_CDC_DESC_SUBTYPE_COUNTRY = 0x07,            // Country Selection Functional Descriptor
    USB_CDC_DESC_SUBTYPE_TEL_MODE = 0x08,           // Telephone Operational Modes Functional Descriptor
    USB_CDC_DESC_SUBTYPE_TERMINAL = 0x09,           // USB Terminal
    USB_CDC_DESC_SUBTYPE_NCHT = 0x0A,               // Network Channel Terminal
    USB_CDC_DESC_SUBTYPE_PROTOCOL = 0x08,           // Protocol Unit
    USB_CDC_DESC_SUBTYPE_EXTENSION = 0x0C,          // Extension Unit
    USB_CDC_DESC_SUBTYPE_MULTI_CHAN = 0x0D,         // Multi-Channel Management Functional Descriptor
    USB_CDC_DESC_SUBTYPE_CAPI = 0x0E,               // CAPI Control
    USB_CDC_DESC_SUBTYPE_ETH = 0x0F,                // Ethernet Networking
    USB_CDC_DESC_SUBTYPE_ATM = 0x10,                // ATM Networking
    USB_CDC_DESC_SUBTYPE_WHANDSET = 0x11,           // Wireless Handset Control Model Functional Descriptor
    USB_CDC_DESC_SUBTYPE_MDLM = 0x12,               // Mobile Direct Line Model
    USB_CDC_DESC_SUBTYPE_MDLM_DETAIL = 0x13,        // MDLM Detail
    USB_CDC_DESC_SUBTYPE_DMM = 0x14,                // Device Management Model
    USB_CDC_DESC_SUBTYPE_OBEX = 0x15,               // OBEX Functional
    USB_CDC_DESC_SUBTYPE_COMMAND_SET = 0x16,        // Command Set
    USB_CDC_DESC_SUBTYPE_COMMAND_SET_DETAIL = 0x17, // Command Set Detail Functional Descriptor
    USB_CDC_DESC_SUBTYPE_TEL_CM = 0x18,             // Telephone Control Model Functional Descriptor
    USB_CDC_DESC_SUBTYPE_OBEX_SERVICE = 0x19,       // OBEX Service Identifier Functional Descriptor
    USB_CDC_DESC_SUBTYPE_NCM = 0x1A,                // NCM Functional Descriptor
    USB_CDC_DESC_SUBTYPE_MAX
} __attribute__((packed)) cdc_desc_subtype_t;

/**
 * @brief USB CDC Subclass codes
 *
 * @see Table 4, USB CDC specification rev. 1.2
 */
typedef enum
{
    USB_CDC_SUBCLASS_DLCM = 0x01,    // Direct Line Control Model
    USB_CDC_SUBCLASS_ACM = 0x02,     // Abstract Control Model
    USB_CDC_SUBCLASS_TCM = 0x03,     // Telephone Control Model
    USB_CDC_SUBCLASS_MCHCM = 0x04,   // Multi-Channel Control Model
    USB_CDC_SUBCLASS_CAPI = 0x05,    // CAPI Control Model
    USB_CDC_SUBCLASS_ECM = 0x06,     // Ethernet Networking Control Model
    USB_CDC_SUBCLASS_ATM = 0x07,     // ATM Networking Model
    USB_CDC_SUBCLASS_HANDSET = 0x08, // Wireless Handset Control Model
    USB_CDC_SUBCLASS_DEV_MAN = 0x09, // Device Management
    USB_CDC_SUBCLASS_MOBILE = 0x0A,  // Mobile Direct Line Model
    USB_CDC_SUBCLASS_OBEX = 0x0B,    // OBEX
    USB_CDC_SUBCLASS_EEM = 0x0C,     // Ethernet Emulation Model
    USB_CDC_SUBCLASS_NCM = 0x0D      // Network Control Model
} __attribute__((packed)) cdc_subclass_t;

/**
 * @brief USB CDC Communications Protocol Codes
 *
 * @see Table 5, USB CDC specification rev. 1.2
 */
typedef enum
{
    USB_CDC_COMM_PROTOCOL_NONE = 0x00,   // No class specific protocol required
    USB_CDC_COMM_PROTOCOL_V250 = 0x01,   // AT Commands: V.250 etc
    USB_CDC_COMM_PROTOCOL_PCAA = 0x02,   // AT Commands defined by PCCA-101
    USB_CDC_COMM_PROTOCOL_PCAA_A = 0x03, // AT Commands defined by PCAA-101 & Annex O
    USB_CDC_COMM_PROTOCOL_GSM = 0x04,    // AT Commands defined by GSM 07.07
    USB_CDC_COMM_PROTOCOL_3GPP = 0x05,   // AT Commands defined by 3GPP 27.007
    USB_CDC_COMM_PROTOCOL_TIA = 0x06,    // AT Commands defined by TIA for CDMA
    USB_CDC_COMM_PROTOCOL_EEM = 0x07,    // Ethernet Emulation Model
    USB_CDC_COMM_PROTOCOL_EXT = 0xFE,    // External Protocol: Commands defined by Command Set Functional Descriptor
    USB_CDC_COMM_PROTOCOL_VENDOR = 0xFF  // Vendor-specific
} __attribute__((packed)) cdc_comm_protocol_t;

/**
 * @brief USB CDC Data Protocol Codes
 *
 * @see Table 7, USB CDC specification rev. 1.2
 */
typedef enum
{
    USB_CDC_DATA_PROTOCOL_NONE = 0x00,   // No class specific protocol required
    USB_CDC_DATA_PROTOCOL_NCM = 0x01,    // Network Transfer Block
    USB_CDC_DATA_PROTOCOL_I430 = 0x30,   // Physical interface protocol for ISDN BRI
    USB_CDC_DATA_PROTOCOL_HDLC = 0x31,   // HDLC
    USB_CDC_DATA_PROTOCOL_Q921M = 0x50,  // Management protocol for Q.921 data link protocol
    USB_CDC_DATA_PROTOCOL_Q921 = 0x51,   // Data link protocol for Q.931
    USB_CDC_DATA_PROTOCOL_Q921TM = 0x52, // TEI-multiplexor for Q.921 data link protocol
    USB_CDC_DATA_PROTOCOL_V42BIS = 0x90, // Data compression procedures
    USB_CDC_DATA_PROTOCOL_Q931 = 0x91,   // Euro-ISDN protocol control
    USB_CDC_DATA_PROTOCOL_V120 = 0x92,   // V.24 rate adaptation to ISDN
    USB_CDC_DATA_PROTOCOL_CAPI = 0x93,   // CAPI Commands
    USB_CDC_DATA_PROTOCOL_VENDOR = 0xFF  // Vendor-specific
} __attribute__((packed)) cdc_data_protocol_t;

/**
 * @brief USB CDC Request Codes
 *
 * @see Table 19, USB CDC specification rev. 1.2
 */
typedef enum
{
    USB_CDC_REQ_SEND_ENCAPSULATED_COMMAND = 0x00,
    USB_CDC_REQ_GET_ENCAPSULATED_RESPONSE = 0x01,
    USB_CDC_REQ_SET_COMM_FEATURE = 0x02,
    USB_CDC_REQ_GET_COMM_FEATURE = 0x03,
    USB_CDC_REQ_CLEAR_COMM_FEATURE = 0x04,
    USB_CDC_REQ_GET_DESCRIPTOR = 0x06,
    USB_CDC_REQ_SET_DESCRIPTOR = 0x07,
    USB_CDC_REQ_SET_AUX_LINE_STATE = 0x10,
    USB_CDC_REQ_SET_HOOK_STATE = 0x11,
    USB_CDC_REQ_PULSE_SETUP = 0x12,
    USB_CDC_REQ_SEND_PULSE = 0x13,
    USB_CDC_REQ_SET_PULSE_TIME = 0x14,
    USB_CDC_REQ_RING_AUX_JACK = 0x15,
    USB_CDC_REQ_SET_LINE_CODING = 0x20,
    USB_CDC_REQ_GET_LINE_CODING = 0x21,
    USB_CDC_REQ_SET_CONTROL_LINE_STATE = 0x22,
    USB_CDC_REQ_SEND_BREAK = 0x23,
    USB_CDC_REQ_SET_RINGER_PARMS = 0x30,
    USB_CDC_REQ_GET_RINGER_PARMS = 0x31,
    USB_CDC_REQ_SET_OPERATION_PARMS = 0x32,
    USB_CDC_REQ_GET_OPERATION_PARMS = 0x33,
    USB_CDC_REQ_SET_LINE_PARMS = 0x34,
    USB_CDC_REQ_GET_LINE_PARMS = 0x35,
    USB_CDC_REQ_DIAL_DIGITS = 0x36,
    USB_CDC_REQ_SET_UNIT_PARAMETER = 0x37,
    USB_CDC_REQ_GET_UNIT_PARAMETER = 0x38,
    USB_CDC_REQ_CLEAR_UNIT_PARAMETER = 0x39,
    USB_CDC_REQ_GET_PROFILE = 0x3A,
    USB_CDC_REQ_SET_ETHERNET_MULTICAST_FILTERS = 0x40,
    USB_CDC_REQ_SET_ETHERNET_POWER_MANAGEMENT_PATTERN_FILTER = 0x41,
    USB_CDC_REQ_GET_ETHERNET_POWER_MANAGEMENT_PATTERN_FILTER = 0x42,
    USB_CDC_REQ_SET_ETHERNET_PACKET_FILTER = 0x43,
    USB_CDC_REQ_GET_ETHERNET_STATISTIC = 0x44,
    USB_CDC_REQ_SET_ATM_DATA_FORMAT = 0x50,
    USB_CDC_REQ_GET_ATM_DEVICE_STATISTICS = 0x51,
    USB_CDC_REQ_SET_ATM_DEFAULT_VC = 0x52,
    USB_CDC_REQ_GET_ATM_VC_STATISTICS = 0x53,
    USB_CDC_REQ_GET_NTB_PARAMETERS = 0x80,
    USB_CDC_REQ_GET_NET_ADDRESS = 0x81,
    USB_CDC_REQ_SET_NET_ADDRESS = 0x82,
    USB_CDC_REQ_GET_NTB_FORMAT = 0x83,
    USB_CDC_REQ_SET_NTB_FORMAT = 0x84,
    USB_CDC_REQ_GET_NTB_INPUT_SIZE = 0x85,
    USB_CDC_REQ_SET_NTB_INPUT_SIZE = 0x86,
    USB_CDC_REQ_GET_MAX_DATAGRAM_SIZE = 0x87,
    USB_CDC_REQ_SET_MAX_DATAGRAM_SIZE = 0x88,
    USB_CDC_REQ_GET_CRC_MODE = 0x89,
    USB_CDC_REQ_SET_CRC_MODE = 0x8A
} __attribute__((packed)) cdc_request_code_t;

/**
 * @brief USB CDC Notification Codes
 *
 * @see Table 20, USB CDC specification rev. 1.2
 */
typedef enum
{
    USB_CDC_NOTIF_NETWORK_CONNECTION = 0x00,
    USB_CDC_NOTIF_RESPONSE_AVAILABLE = 0x01,
    USB_CDC_NOTIF_AUX_JACK_HOOK_STATE = 0x08,
    USB_CDC_NOTIF_RING_DETECT = 0x09,
    USB_CDC_NOTIF_SERIAL_STATE = 0x20,
    USB_CDC_NOTIF_CALL_STATE_CHANGE = 0x28,
    USB_CDC_NOTIF_LINE_STATE_CHANGE = 0x29,
    USB_CDC_NOTIF_CONNECTION_SPEED_CHANGE = 0x2A
} __attribute__((packed)) cdc_notification_code_t;

typedef struct
{
    uint8_t bmRequestType;
    cdc_notification_code_t bNotificationCode;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
    uint8_t Data[];
} __attribute__((packed)) cdc_notification_t;

/**
 * @brief USB CDC Header Functional Descriptor
 *
 * @see Table 15, USB CDC specification rev. 1.2
 */
typedef struct
{
    uint8_t bFunctionLength;
    const uint8_t bDescriptorType; // Upper nibble: CDC code 0x02, Lower nibble: intf/ep descriptor type 0x04/0x05
    const cdc_desc_subtype_t bDescriptorSubtype;
    uint16_t bcdCDC; // CDC version as binary-coded decimal. This driver is written for version 1.2
} __attribute__((packed)) cdc_header_desc_t;

/**
 * @brief USB CDC Union Functional Descriptor
 *
 * @see Table 16, USB CDC specification rev. 1.2
 */
typedef struct
{
    uint8_t bFunctionLength;
    const uint8_t bDescriptorType; // Upper nibble: CDC code 0x02, Lower nibble: intf/ep descriptor type 0x04/0x05
    const cdc_desc_subtype_t bDescriptorSubtype;
    const uint8_t bControlInterface; // Master/controlling interface
    uint8_t bSubordinateInterface[]; // Slave/subordinate interfaces
} __attribute__((packed)) cdc_union_desc_t;

/**
 * @brief USB CDC Ethernet Functional Descriptor
 *
 * @see Table 16, USB CDC specification rev. 1.2
 */
typedef struct
{
    uint8_t bFunctionLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint8_t iMACAddress;
    uint32_t bmEthernetStatistics;
    uint16_t wMaxSegmentSize;
    uint16_t wNumberMCFilters;
    uint8_t bNumberPowerFilters;
} __attribute__((packed)) cdc_ecm_eth_desc_t;
/**
 * @brief USB CDC PSTN Call Descriptor
 *
 * @see Table 3, USB CDC-PSTN specification rev. 1.2
 */
typedef struct
{
    uint8_t bFunctionLength;
    const uint8_t bDescriptorType;
    const cdc_desc_subtype_t bDescriptorSubtype;
    union
    {
        struct
        {
            uint8_t call_management : 1;   // Device handles call management itself
            uint8_t call_over_data_if : 1; // Device sends/receives call management information over Data Class interface
            uint8_t reserved : 6;
        };
        uint8_t val;
    } bmCapabilities;
    uint8_t bDataInterface; // Interface number of Data Class interface optionally used for call management
} __attribute__((packed)) cdc_acm_call_desc_t;

/**
 * @brief USB CDC PSTN Abstract Control Model Descriptor
 *
 * @see Table 4, USB CDC-PSTN specification rev. 1.2
 */
typedef struct
{
    uint8_t bFunctionLength;
    const uint8_t bDescriptorType;
    const cdc_desc_subtype_t bDescriptorSubtype;
    union
    {
        struct
        {
            uint8_t feature : 1;    // Device supports Set/Clear/Get_Comm_Feature requests
            uint8_t serial : 1;     // Device supports Set/Get_Line_Coding, Set_Control_Line_State and Serial_State request and notifications
            uint8_t send_break : 1; // Device supports Send_Break request
            uint8_t network : 1;    // Device supports Network_Connection notification
            uint8_t reserved : 4;
        };
        uint8_t val;
    } bmCapabilities;
} __attribute__((packed)) cdc_acm_acm_desc_t;

/**
 * @brief Line Coding structure
 * @see Table 17, USB CDC-PSTN specification rev. 1.2
 */
typedef struct
{
    uint32_t dwDTERate;  // in bits per second
    uint8_t bCharFormat; // 0: 1 stopbit, 1: 1.5 stopbits, 2: 2 stopbits
    uint8_t bParityType; // 0: None, 1: Odd, 2: Even, 3: Mark, 4: Space
    uint8_t bDataBits;   // 5, 6, 7, 8 or 16
} __attribute__((packed)) cdc_acm_line_coding_t;

/**
 * @brief UART State Bitmap
 * @see Table 31, USB CDC-PSTN specification rev. 1.2
 */
typedef union
{
    struct
    {
        uint16_t bRxCarrier : 1;  // State of receiver carrier detection mechanism of device. This signal corresponds to V.24 signal 109 and RS-232 signal DCD.
        uint16_t bTxCarrier : 1;  // State of transmission carrier. This signal corresponds to V.24 signal 106 and RS-232 signal DSR.
        uint16_t bBreak : 1;      // State of break detection mechanism of the device.
        uint16_t bRingSignal : 1; // State of ring signal detection of the device.
        uint16_t bFraming : 1;    // A framing error has occurred.
        uint16_t bParity : 1;     // A parity error has occurred.
        uint16_t bOverRun : 1;    // Received data has been discarded due to overrun in the device.
        uint16_t reserved : 9;
    };
    uint16_t val;
} cdc_acm_uart_state_t;
