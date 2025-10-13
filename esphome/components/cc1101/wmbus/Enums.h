#include <cstdint>
#include <vector>
#pragma once
enum WmBusFrameType : uint8_t {
	WMBUS_FRAME_UNKNOWN = 0,
	WMBUS_FRAMEA = 1,
	WMBUS_FRAMEB = 2,
};

enum WmBusFrameMode : uint8_t {
	WMBUS_UNKNOWN_MODE = 0,
	WMBUS_T1_MODE = 1,
	WMBUS_C1_MODE = 2,
	WMBUS_T1C1_MODE = 3,
};

typedef struct RXinfoDescr {
	uint8_t  lengthField;         // The L-field in the WMBUS packet
	uint16_t length;              // Total number of bytes to to be read from the RX FIFO
	uint16_t bytesLeft;           // Bytes left to to be read from the RX FIFO
	uint8_t* pByteIndex;          // Pointer to current position in the byte array
	bool start;                   // Start of Packet
	bool complete;                // Packet received complete
	uint8_t state;
	WmBusFrameMode framemode;
	WmBusFrameType frametype;
} RXinfoDescr;

typedef struct WMbusFrame {
	std::vector<unsigned char> frame{};
	WmBusFrameMode framemode;
	int8_t rssi;
	uint8_t lqi;
} WMbusFrame;