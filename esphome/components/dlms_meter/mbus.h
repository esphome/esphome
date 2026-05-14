#pragma once

#include <cstdint>

namespace esphome::dlms_meter {

/*
+----------------------------------------------------+ -
|               Start Character [0x68]               |  \
+----------------------------------------------------+   |
|                   Data Length (L)                  |   |
+----------------------------------------------------+   |
|               Data Length Repeat (L)               |   |
+----------------------------------------------------+    > M-Bus Data link layer
|            Start Character Repeat [0x68]           |   |
+----------------------------------------------------+   |
|             Control/Function Field (C)             |   |
+----------------------------------------------------+   |
|                  Address Field (A)                 |  /
+----------------------------------------------------+ -
|           Control Information Field (CI)           |  \
+----------------------------------------------------+   |
|    Source Transport Service Access Point (STSAP)   |    > DLMS/COSEM M-Bus transport layer
+----------------------------------------------------+   |
| Destination Transport Service Access Point (DTSAP) |  /
+----------------------------------------------------+ -
|                                                    |  \
~                                                    ~   |
                         Data                             > DLMS/COSEM Application Layer
~                                                    ~   |
|                                                    |  /
+----------------------------------------------------+ -
|                      Checksum                      |  \
+----------------------------------------------------+    > M-Bus Data link layer
|                Stop Character [0x16]               |  /
+----------------------------------------------------+ -

Data_Length = L - C - A - CI
Each line (except Data) is one Byte

Possible Values found in publicly available docs:
- C: 0x53/0x73 (SND_UD)
- A: FF (Broadcast)
- CI: 0x00-0x1F/0x60/0x61/0x7C/0x7D
- STSAP: 0x01 (Management Logical Device ID 1 of the meter)
- DTSAP: 0x67 (Consumer Information Push Client ID 103)
 */

// MBUS start bytes for different telegram formats:
// - Single Character: 0xE5 (length=1)
// - Short Frame: 0x10 (length=5)
// - Control Frame: 0x68 (length=9)
// - Long Frame: 0x68 (length=9+data_length)
// This component currently only uses Long Frame.
static constexpr uint8_t START_BYTE_SINGLE_CHARACTER = 0xE5;
static constexpr uint8_t START_BYTE_SHORT_FRAME = 0x10;
static constexpr uint8_t START_BYTE_CONTROL_FRAME = 0x68;
static constexpr uint8_t START_BYTE_LONG_FRAME = 0x68;
static constexpr uint8_t MBUS_HEADER_INTRO_LENGTH = 4;  // Header length for the intro (0x68, length, length, 0x68)
static constexpr uint8_t MBUS_FULL_HEADER_LENGTH = 9;   // Total header length
static constexpr uint8_t MBUS_FOOTER_LENGTH = 2;        // Footer after frame
static constexpr uint8_t MBUS_MAX_FRAME_LENGTH = 250;   // Maximum size of frame
static constexpr uint8_t MBUS_START1_OFFSET = 0;        // Offset of first start byte
static constexpr uint8_t MBUS_LENGTH1_OFFSET = 1;       // Offset of first length byte
static constexpr uint8_t MBUS_LENGTH2_OFFSET = 2;       // Offset of (duplicated) second length byte
static constexpr uint8_t MBUS_START2_OFFSET = 3;        // Offset of (duplicated) second start byte
static constexpr uint8_t STOP_BYTE = 0x16;

}  // namespace esphome::dlms_meter
