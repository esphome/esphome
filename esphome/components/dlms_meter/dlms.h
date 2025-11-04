#pragma once

#include <cstdint>

namespace esphome {
namespace dlms_meter {

/*
 * Data structure
 */

/*
+-------------------------------+
|       Ciphering Service       |
+-------------------------------+
|      System Title Length      |
+-------------------------------+
|                               |
|                               |
|                               |
|            System             |
|            Title              |
|                               |
|                               |
|                               |
+-------------------------------+
|            Length             | (1 or 3 Bytes)
+-------------------------------+
|     Security Control Byte     |
+-------------------------------+
|                               |
|             Frame             |
|            Counter            |
|                               |
+-------------------------------+
|                               |
~                               ~
        Encrypted Payload
~                               ~
|                               |
+-------------------------------+

Ciphering Service: 0xDB (General-Glo-Ciphering)
System Title Length: 0x08
System Title: Unique ID of meter
Length: 1 Byte=Length <= 127, 3 Bytes=Length > 127 (0x82 & 2 Bytes length)
Security Control Byte:
- Bit 3…0: Security_Suite_Id
- Bit 4: "A" subfield: indicates that authentication is applied
- Bit 5: "E" subfield: indicates that encryption is applied
- Bit 6: Key_Set subfield: 0 = Unicast, 1 = Broadcast
- Bit 7: Indicates the use of compression.
 */

static constexpr uint8_t DLMS_HEADER_LENGTH = 16;  // Length of the header (total message length <= 127)
static constexpr uint8_t DLMS_HEADER_EXT_OFFSET =
    2;  // Length to offset when header is extended length (total message length > 127)

static constexpr uint8_t DLMS_CIPHER_OFFSET = 0;  // Offset at which used cipher suite is stored
static constexpr uint8_t DLMS_SYST_OFFSET = 1;    // Offset at which length of system title is stored

static constexpr uint8_t DLMS_LENGTH_OFFSET = 10;  // Offset at which message length is stored
static constexpr uint8_t TWO_BYTE_LENGTH = 0x82;   // Two byte length indicator / prefix
static constexpr uint8_t DLMS_LENGTH_CORRECTION =
    5;  // Part of the header is included in the DLMS length field and needs to be removed

// Bytes after length may be shifted depending on length field

static constexpr uint8_t DLMS_SECBYTE_OFFSET = 11;  // Offset of the security byte

static constexpr uint8_t DLMS_FRAMECOUNTER_OFFSET = 12;  // Offset of the frame counter
static constexpr uint8_t DLMS_FRAMECOUNTER_LENGTH = 4;   // Length of the frame counter (always 4)

static constexpr uint8_t DLMS_PAYLOAD_OFFSET = 16;  // Offset at which the encrypted payload

static constexpr uint8_t GLO_CIPHERING = 0xDB;
static constexpr uint8_t DATA_NOTIFICATION = 0x0F;
static constexpr uint8_t TIMESTAMP_DATETIME = 0x0C;

/*
 * provider specific quirks
 */

static constexpr uint8_t NETZ_NOE_MAGIC_BYTE = 0x81;  // Magic length byte used by Netz NOE
static constexpr uint8_t NETZ_NOE_EXPECTED_MESSAGE_LENGTH = 0xF8;

}  // namespace dlms_meter
}  // namespace esphome
