#include "Arduino.h"

#ifdef FREQUENCY_433
	#define OPERATING_FREQUENCY 410
#elif defined(FREQUENCY_400)
	#define OPERATING_FREQUENCY 410
#elif defined(FREQUENCY_230)
	#define OPERATING_FREQUENCY 220
#elif defined(FREQUENCY_868)
	#define OPERATING_FREQUENCY 850
#elif defined(FREQUENCY_900)
	#define OPERATING_FREQUENCY 850
#elif defined(FREQUENCY_915)
	#define OPERATING_FREQUENCY 900
#else
	#define OPERATING_FREQUENCY 410
#endif

#define BROADCAST_ADDRESS 255

typedef enum RESPONSE_STATUS {
#ifndef ARDUINO_ARCH_STM32
  SUCCESS = 1,
#endif
  E220_SUCCESS = 1,
  ERR_E220_UNKNOWN,	/* something shouldn't happened */
  ERR_E220_NOT_SUPPORT,
  ERR_E220_NOT_IMPLEMENT,
  ERR_E220_NOT_INITIAL,
  ERR_E220_INVALID_PARAM,
  ERR_E220_DATA_SIZE_NOT_MATCH,
  ERR_E220_BUF_TOO_SMALL,
  ERR_E220_TIMEOUT,
  ERR_E220_HARDWARE,
  ERR_E220_HEAD_NOT_RECOGNIZED,
  ERR_E220_NO_RESPONSE_FROM_DEVICE,
  ERR_E220_WRONG_UART_CONFIG,
  ERR_E220_WRONG_FORMAT,
  ERR_E220_PACKET_TOO_BIG
} Status;

static String getResponseDescriptionByParams(byte status){
	switch (status)
	{
	  case E220_SUCCESS:
		return F("Success");
		break;
	  case ERR_E220_UNKNOWN:
		return F("Unknown");
		break;
	  case ERR_E220_NOT_SUPPORT:
		return F("Not support!");
		break;
	  case ERR_E220_NOT_IMPLEMENT:
		return F("Not implement");
		break;
	  case ERR_E220_NOT_INITIAL:
		return F("Not initial!");
		break;
	  case ERR_E220_INVALID_PARAM:
		return F("Invalid param!");
		break;
	  case ERR_E220_DATA_SIZE_NOT_MATCH:
		return F("Data size not match!");
		break;
	  case ERR_E220_BUF_TOO_SMALL:
		return F("Buff too small!");
		break;
	  case ERR_E220_TIMEOUT:
		return F("Timeout!!");
		break;
	  case ERR_E220_HARDWARE:
		return F("Hardware error!");
		break;
	  case ERR_E220_HEAD_NOT_RECOGNIZED:
		return F("Save mode returned not recognized!");
		break;
	  case ERR_E220_NO_RESPONSE_FROM_DEVICE:
		return F("No response from device! (Check wiring)");
		break;
	  case ERR_E220_WRONG_UART_CONFIG:
		return F("Wrong UART configuration! (BPS must be 9600 for configuration)");
		break;
	  case ERR_E220_PACKET_TOO_BIG:
		return F("The device support only 200byte of data transmission!");
		break;
	  default:
		return F("Invalid status!");
	}
}

enum E220_UART_PARITY
{
  MODE_00_8N1 = 0b00,
  MODE_01_8O1 = 0b01,
  MODE_10_8E1 = 0b10,
  MODE_11_8N1 = 0b11
};

static String getUARTParityDescriptionByParams(byte uartParity){
	switch (uartParity)
	{
	  case MODE_00_8N1:
		return F("8N1 (Default)");
		break;
	  case MODE_01_8O1:
		return F("8O1");
		break;
	  case MODE_10_8E1:
		return F("8E1");
		break;
	  case MODE_11_8N1:
		return F("8N1 (equal to 00");
		break;
	  default:
		return F("Invalid UART Parity!");
	}
}

enum UART_BPS_TYPE
{
  UART_BPS_1200 = 0b000,
  UART_BPS_2400 = 0b001,
  UART_BPS_4800 = 0b010,
  UART_BPS_9600 = 0b011,
  UART_BPS_19200 = 0b100,
  UART_BPS_38400 = 0b101,
  UART_BPS_57600 = 0b110,
  UART_BPS_115200 = 0b111
};

enum UART_BPS_RATE
{
  UART_BPS_RATE_1200 = 1200,
  UART_BPS_RATE_2400 = 2400,
  UART_BPS_RATE_4800 = 4800,
  UART_BPS_RATE_9600 = 9600,
  UART_BPS_RATE_19200 = 19200,
  UART_BPS_RATE_38400 = 38400,
  UART_BPS_RATE_57600 = 57600,
  UART_BPS_RATE_115200 = 115200
};

static String getUARTBaudRateDescriptionByParams(byte uartBaudRate)
{
	switch (uartBaudRate)
	{
	  case UART_BPS_1200:
		return F("1200bps");
		break;
	  case UART_BPS_2400:
		return F("2400bps");
		break;
	  case UART_BPS_4800:
		return F("4800bps");
		break;
	  case UART_BPS_9600:
		return F("9600bps (default)");
		break;
	  case UART_BPS_19200:
		return F("19200bps");
		break;
	  case UART_BPS_38400:
		return F("38400bps");
		break;
	  case UART_BPS_57600:
		return F("57600bps");
		break;
	  case UART_BPS_115200:
		return F("115200bps");
		break;
	  default:
		return F("Invalid UART Baud Rate!");
	}
}

enum AIR_DATA_RATE
{
  AIR_DATA_RATE_000_24 = 0b000,
  AIR_DATA_RATE_001_24 = 0b001,
  AIR_DATA_RATE_010_24 = 0b010,
  AIR_DATA_RATE_011_48 = 0b011,
  AIR_DATA_RATE_100_96 = 0b100,
  AIR_DATA_RATE_101_192 = 0b101,
  AIR_DATA_RATE_110_384 = 0b110,
  AIR_DATA_RATE_111_625 = 0b111
};


static String getAirDataRateDescriptionByParams(byte airDataRate)
{
	switch (airDataRate)
	{
	  case AIR_DATA_RATE_000_24:
		return F("2.4kbps");
		break;
	  case AIR_DATA_RATE_001_24:
		return F("2.4kbps");
		break;
	  case AIR_DATA_RATE_010_24:
		return F("2.4kbps (default)");
		break;
	  case AIR_DATA_RATE_011_48:
		return F("4.8kbps");
		break;
	  case AIR_DATA_RATE_100_96:
		return F("9.6kbps");
		break;
	  case AIR_DATA_RATE_101_192:
		return F("19.2kbps");
		break;
	  case AIR_DATA_RATE_110_384:
		return F("38.4kbps");
		break;
	  case AIR_DATA_RATE_111_625:
		return F("62.5kbps");
		break;
	  default:
		return F("Invalid Air Data Rate!");
	}
}

enum SUB_PACKET_SETTING {
	SPS_200_00 = 0b00,
	SPS_128_01 = 0b01,
	SPS_064_10 = 0b10,
	SPS_032_11 = 0b11

};
static String getSubPacketSettingByParams(byte subPacketSetting)
{
	switch (subPacketSetting)
	{
	  case SPS_200_00:
		return F("200bytes (default)");
		break;
	  case SPS_128_01:
		  return F("128bytes");
		break;
	  case SPS_064_10:
		return F("64bytes");
		break;
	  case SPS_032_11:
		return F("32bytes");
		break;
	  default:
		return F("Invalid Sub Packet Setting!");
	}
}

enum RSSI_AMBIENT_NOISE_ENABLE {
	RSSI_AMBIENT_NOISE_ENABLED = 0b1,
	RSSI_AMBIENT_NOISE_DISABLED = 0b0
};
static String getRSSIAmbientNoiseEnableByParams(byte rssiAmbientNoiseEnabled)
{
	switch (rssiAmbientNoiseEnabled)
	{
	  case RSSI_AMBIENT_NOISE_ENABLED:
		return F("Enabled");
		break;
	  case RSSI_AMBIENT_NOISE_DISABLED:
		  return F("Disabled (default)");
		break;
	  default:
		return F("Invalid RSSI Ambient Noise enabled!");
	}
}

enum WOR_PERIOD {
	WOR_500_000 = 0b000,
	WOR_1000_001 = 0b001,
	WOR_1500_010 = 0b010,
	WOR_2000_011 = 0b011,
	WOR_2500_100 = 0b100,
	WOR_3000_101 = 0b101,
	WOR_3500_110 = 0b110,
	WOR_4000_111 = 0b111

};
static String getWORPeriodByParams(byte WORPeriod)
{
	switch (WORPeriod)
	{
	  case WOR_500_000:
		return F("500ms");
		break;
	  case WOR_1000_001:
			return F("1000ms");
		break;
	  case WOR_1500_010:
			return F("1500ms");
		break;
	  case WOR_2000_011:
			return F("2000ms (default)");
		break;
	  case WOR_2500_100:
			return F("2500ms");
		break;
	  case WOR_3000_101:
			return F("3000ms");
		break;
	  case WOR_3500_110:
			return F("3500ms");
		break;
	  case WOR_4000_111:
			return F("4000ms");
		break;
	  default:
		return F("Invalid WOR period!");
	}
}
enum LBT_ENABLE_BYTE {
	LBT_ENABLED = 0b1,
	LBT_DISABLED = 0b0
};
static String getLBTEnableByteByParams(byte LBTEnableByte)
{
	switch (LBTEnableByte)
	{
	  case LBT_ENABLED:
		return F("Enabled");
		break;
	  case LBT_DISABLED:
		  return F("Disabled (default)");
		break;
	  default:
		return F("Invalid LBT enable byte!");
	}
}

enum RSSI_ENABLE_BYTE {
	RSSI_ENABLED = 0b1,
	RSSI_DISABLED = 0b0
};
static String getRSSIEnableByteByParams(byte RSSIEnableByte)
{
	switch (RSSIEnableByte)
	{
	  case RSSI_ENABLED:
		return F("Enabled");
		break;
	  case RSSI_DISABLED:
		  return F("Disabled (default)");
		break;
	  default:
		return F("Invalid RSSI enable byte!");
	}
}

enum FIDEX_TRANSMISSION
{
  FT_TRANSPARENT_TRANSMISSION = 0b0,
  FT_FIXED_TRANSMISSION = 0b1
};


static String getFixedTransmissionDescriptionByParams(byte fixedTransmission)
{
	switch (fixedTransmission)
	{
	  case FT_TRANSPARENT_TRANSMISSION:
		return F("Transparent transmission (default)");
		break;
	  case FT_FIXED_TRANSMISSION:
		return F("Fixed transmission (first three bytes can be used as high/low address and channel)");
		break;
	  default:
		return F("Invalid fixed transmission param!");
	}
}

#ifdef E220_22
	enum TRANSMISSION_POWER
	{
	  POWER_22 = 0b00,
	  POWER_17 = 0b01,
	  POWER_13 = 0b10,
	  POWER_10 = 0b11

	};

	static String getTransmissionPowerDescriptionByParams(byte transmissionPower)
	{
		switch (transmissionPower)
		{
		  case POWER_22:
			return F("22dBm (Default)");
			break;
		  case POWER_17:
			return F("17dBm");
			break;
		  case POWER_13:
			return F("13dBm");
			break;
		  case POWER_10:
			return F("10dBm");
			break;
		  default:
			return F("Invalid transmission power param");
		}
	}
#elif defined(E220_30)
	enum TRANSMISSION_POWER
	{
	  POWER_30 = 0b00,
	  POWER_27 = 0b01,
	  POWER_24 = 0b10,
	  POWER_21 = 0b11

	};

	static String getTransmissionPowerDescriptionByParams(byte transmissionPower)
	{
		switch (transmissionPower)
		{
		  case POWER_30:
			return F("30dBm (Default)");
			break;
		  case POWER_27:
			return F("27dBm");
			break;
		  case POWER_24:
			return F("24dBm");
			break;
		  case POWER_21:
			return F("21dBm");
			break;
		  default:
			return F("Invalid transmission power param");
		}
	}
#else
	enum TRANSMISSION_POWER
	{
	  POWER_22 = 0b00,
	  POWER_17 = 0b01,
	  POWER_13 = 0b10,
	  POWER_10 = 0b11

	};

	static String getTransmissionPowerDescriptionByParams(byte transmissionPower)
	{
		switch (transmissionPower)
		{
		  case POWER_22:
			return F("22dBm (Default)");
			break;
		  case POWER_17:
			return F("17dBm");
			break;
		  case POWER_13:
			return F("13dBm");
			break;
		  case POWER_10:
			return F("10dBm");
			break;
		  default:
			return F("Invalid transmission power param");
		}
	}
#endif
