#ifndef LoRa_E220_h
#define LoRa_E220_h

#if  !USE_ESP32 && !defined(__STM32F1__) && !defined(__STM32F4__)
	#define ACTIVATE_SOFTWARE_SERIAL
#endif
#ifdef USE_ESP32
	#define HARDWARE_SERIAL_SELECTABLE_PIN
#endif

#ifdef ACTIVATE_SOFTWARE_SERIAL
	#include <SoftwareSerial.h>
#endif

#include "state_naming.h"

#include "Arduino.h"


#define MAX_SIZE_TX_PACKET 200

// Uncomment to enable printing out nice debug messages.
//#define LoRa_E220_DEBUG

// Define where debug output will be printed.
#define DEBUG_PRINTER Serial

// Setup debug printing macros.
#ifdef LoRa_E220_DEBUG
	#define DEBUG_PRINT(...) { DEBUG_PRINTER.print(__VA_ARGS__); }
	#define DEBUG_PRINTLN(...) { DEBUG_PRINTER.println(__VA_ARGS__); }
#else
	#define DEBUG_PRINT(...) {}
	#define DEBUG_PRINTLN(...) {}
#endif

enum MODE_TYPE {
	MODE_0_NORMAL 			= 0,
	MODE_0_TRANSMISSION 	= 0,
	MODE_1_WOR_TRANSMITTER 	= 1,
	MODE_1_WOR				= 1,
	MODE_2_WOR_RECEIVER 	= 2,
	MODE_2_POWER_SAVING 	= 2,
	MODE_3_CONFIGURATION 	= 3,
	MODE_3_PROGRAM 			= 3,
	MODE_3_SLEEP 			= 3,
	MODE_INIT 				= 0xFF
};

enum PROGRAM_COMMAND {
	WRITE_CFG_PWR_DWN_SAVE 	= 0xC0,
	READ_CONFIGURATION 		= 0xC1,
	WRITE_CFG_PWR_DWN_LOSE 	= 0xC2,
	WRONG_FORMAT 			= 0xFF,
	RETURNED_COMMAND 		= 0xC1,
	SPECIAL_WIFI_CONF_COMMAND = 0xCF
};

enum REGISTER_ADDRESS {
	REG_ADDRESS_CFG			= 0x00,
	REG_ADDRESS_SPED 		= 0x02,
	REG_ADDRESS_TRANS_MODE 	= 0x03,
	REG_ADDRESS_CHANNEL 	= 0x04,
	REG_ADDRESS_OPTION	 	= 0x05,
	REG_ADDRESS_CRYPT	 	= 0x06,
	REG_ADDRESS_PID		 	= 0x08
};

enum PACKET_LENGHT {
	PL_CONFIGURATION 	= 0x08,

	PL_SPED				= 0x01,
	PL_OPTION			= 0x01,
	PL_TRANSMISSION_MODE= 0x01,
	PL_CHANNEL			= 0x01,
	PL_CRYPT			= 0x02,
	PL_PID				= 0x03
};

#pragma pack(push, 1)
struct Speed {
	uint8_t airDataRate :3; //bit 0-2
	std::string getAirDataRateDescription() {
		return getAirDataRateDescriptionByParams(this->airDataRate);
	}

	uint8_t uartParity :2; //bit 3-4
	std::string getUARTParityDescription() {
		return getUARTParityDescriptionByParams(this->uartParity);
	}

	uint8_t uartBaudRate :3; //bit 5-7
	std::string getUARTBaudRateDescription() {
		return getUARTBaudRateDescriptionByParams(this->uartBaudRate);
	}

};

struct TransmissionMode {
	byte WORPeriod :3; //bit 2,1,0
	std::string getWORPeriodByParamsDescription() {
		return getWORPeriodByParams(this->WORPeriod);
	}
	byte reserved2 :1; //bit 3
	byte enableLBT :1; //bit 4
	std::string getLBTEnableByteDescription() {
		return getLBTEnableByteByParams(this->enableLBT);
	}
	byte reserved :1; //bit 5

	byte fixedTransmission :1; //bit 6
	std::string getFixedTransmissionDescription() {
		return getFixedTransmissionDescriptionByParams(this->fixedTransmission);
	}

	byte enableRSSI :1; //bit 7
	std::string getRSSIEnableByteDescription() {
		return getRSSIEnableByteByParams(this->enableRSSI);
	}
};

struct Option {
	uint8_t transmissionPower :2; //bit 0-1
	std::string getTransmissionPowerDescription() {
		return getTransmissionPowerDescriptionByParams(this->transmissionPower);
	}
	uint8_t reserved :3; //bit 2-4

	uint8_t RSSIAmbientNoise :1; //bit 5
	std::string getRSSIAmbientNoiseEnable() {
		return getRSSIAmbientNoiseEnableByParams(this->RSSIAmbientNoise);
	}

	uint8_t subPacketSetting :2; //bit 6-7
	std::string getSubPacketSetting() {
		return getSubPacketSettingByParams(this->subPacketSetting);
	}

};

struct Crypt {
	byte CRYPT_H = 0;
	byte CRYPT_L = 0;
};

struct Configuration {
	byte COMMAND = 0;
	byte STARTING_ADDRESS = 0;
	byte LENGHT = 0;

	byte ADDH = 0;
	byte ADDL = 0;

	struct Speed SPED;
	struct Option OPTION;

	byte CHAN = 0;
	std::string getChannelDescription() {
		return std::string(this->CHAN + OPERATING_FREQUENCY) + F("MHz");
	}

	struct TransmissionMode TRANSMISSION_MODE;

	struct Crypt CRYPT;
};

struct ModuleInformation {
	byte COMMAND = 0;
	byte STARTING_ADDRESS = 0;
	byte LENGHT = 0;

	byte model = 0;
	byte version = 0;
	byte features = 0;
};

struct ResponseStatus {
	std::std::string code;
	std::std::string getResponseDescription() {
		return getResponseDescriptionByParams(this->code);
	}
};

struct ResponseStructContainer {
	void *data;
	byte rssi;
	ResponseStatus status;
	void close() {
		free(this->data);
	}
};
struct ResponseContainer {
	std::string data;
	byte rssi;
	ResponseStatus status;
};

struct ConfigurationMessage
{
	byte specialCommand1 = 0xCF;
	byte specialCommand2 = 0xCF;

	unsigned char message[];
};

//struct FixedStransmission {
//		byte ADDL = 0;
//		byte ADDH = 0;
//		byte CHAN = 0;
//		void *message;
//};
#pragma pack(pop)

class LoRa_E220 {
	public:
#ifdef ACTIVATE_SOFTWARE_SERIAL
		LoRa_E220(byte txE220pin, byte rxE220pin, UART_BPS_RATE bpsRate = UART_BPS_RATE_9600);
		LoRa_E220(byte txE220pin, byte rxE220pin, byte auxPin, UART_BPS_RATE bpsRate = UART_BPS_RATE_9600);
		LoRa_E220(byte txE220pin, byte rxE220pin, byte auxPin, byte m0Pin, byte m1Pin, UART_BPS_RATE bpsRate = UART_BPS_RATE_9600);
#endif

		LoRa_E220(HardwareSerial* serial, UART_BPS_RATE bpsRate = UART_BPS_RATE_9600);
		LoRa_E220(HardwareSerial* serial, byte auxPin, UART_BPS_RATE bpsRate = UART_BPS_RATE_9600);
		LoRa_E220(HardwareSerial* serial, byte auxPin, byte m0Pin, byte m1Pin, UART_BPS_RATE bpsRate = UART_BPS_RATE_9600);

#ifdef HARDWARE_SERIAL_SELECTABLE_PIN
		LoRa_E220(byte txE220pin, byte rxE220pin, HardwareSerial* serial, UART_BPS_RATE bpsRate, uint32_t serialConfig = SERIAL_8N1);
		LoRa_E220(byte txE220pin, byte rxE220pin, HardwareSerial* serial, byte auxPin, UART_BPS_RATE bpsRate, uint32_t serialConfig = SERIAL_8N1);
		LoRa_E220(byte txE220pin, byte rxE220pin, HardwareSerial* serial, byte auxPin, byte m0Pin, byte m1Pin, UART_BPS_RATE bpsRate, uint32_t serialConfig = SERIAL_8N1);
#endif

#ifdef ACTIVATE_SOFTWARE_SERIAL
		LoRa_E220(SoftwareSerial* serial, UART_BPS_RATE bpsRate = UART_BPS_RATE_9600);
		LoRa_E220(SoftwareSerial* serial, byte auxPin, UART_BPS_RATE bpsRate = UART_BPS_RATE_9600);
		LoRa_E220(SoftwareSerial* serial, byte auxPin, byte m0Pin, byte m1Pin, UART_BPS_RATE bpsRate = UART_BPS_RATE_9600);
#endif

//		LoRa_E220(byte txE220pin, byte rxE220pin, UART_BPS_RATE bpsRate = UART_BPS_RATE_9600, MODE_TYPE mode = MODE_0_NORMAL);
//		LoRa_E220(HardwareSerial* serial = &Serial, UART_BPS_RATE bpsRate = UART_BPS_RATE_9600, MODE_TYPE mode = MODE_0_NORMAL);
//		LoRa_E220(SoftwareSerial* serial, UART_BPS_RATE bpsRate = UART_BPS_RATE_9600, MODE_TYPE mode = MODE_0_NORMAL);

		bool begin();
        Status setMode(MODE_TYPE mode);
        MODE_TYPE getMode();

		ResponseStructContainer getConfiguration();
		ResponseStatus setConfiguration(Configuration configuration, PROGRAM_COMMAND saveType = WRITE_CFG_PWR_DWN_LOSE);

		ResponseStructContainer getModuleInformation();
		ResponseStatus resetModule();

		ResponseStatus sendMessage(const void *message, const uint8_t size);

	    ResponseContainer receiveMessageUntil(char delimiter = '\0');
		ResponseStructContainer receiveMessage(const uint8_t size);
		ResponseStructContainer receiveMessageRSSI(const uint8_t size);
	        
        ResponseStructContainer receiveMessageComplete(const uint8_t size, bool enableRSSI);
		ResponseContainer receiveMessageComplete(bool enableRSSI);
	
		ResponseStatus sendMessage(const std::string message);
		ResponseContainer receiveMessage();
		ResponseContainer receiveMessageRSSI();

		ResponseStatus sendFixedMessage(byte ADDH, byte ADDL, byte CHAN, const std::string message);

        ResponseStatus sendFixedMessage(byte ADDH,byte ADDL, byte CHAN, const void *message, const uint8_t size);
        ResponseStatus sendBroadcastFixedMessage(byte CHAN, const void *message, const uint8_t size);
        ResponseStatus sendBroadcastFixedMessage(byte CHAN, const std::string message);

		ResponseContainer receiveInitialMessage(const uint8_t size);

		ResponseStatus sendConfigurationMessage( byte ADDH,byte ADDL, byte CHAN, Configuration *configuration, PROGRAM_COMMAND programCommand = WRITE_CFG_PWR_DWN_SAVE);

        int available();
	private:
		HardwareSerial* hs;

#ifdef ACTIVATE_SOFTWARE_SERIAL
		SoftwareSerial* ss;
#endif

		bool isSoftwareSerial = true;

		int8_t txE220pin = -1;
		int8_t rxE220pin = -1;
		int8_t auxPin = -1;

#ifdef HARDWARE_SERIAL_SELECTABLE_PIN
		uint32_t serialConfig = SERIAL_8N1;
#endif

		int8_t m0Pin = -1;
		int8_t m1Pin = -1;

		unsigned long halfKeyloqKey = 0x06660708;
		unsigned long encrypt(unsigned long data);
		unsigned long decrypt(unsigned long data);

		UART_BPS_RATE bpsRate = UART_BPS_RATE_9600;

		struct NeedsStream {
			template<typename T>
			void begin(T &t, uint32_t baud) {
				DEBUG_PRINTLN("Begin ");
				t.setTimeout(500);
				t.begin(baud);
				stream = &t;
			}

#ifdef HARDWARE_SERIAL_SELECTABLE_PIN
//		  template< typename T >
//		  void begin( T &t, uint32_t baud, SerialConfig config ){
//			  DEBUG_PRINTLN("Begin ");
//			  t.setTimeout(500);
//			  t.begin(baud, config);
//			  stream = &t;
//		  }
//
			template< typename T >
			void begin( T &t, uint32_t baud, uint32_t config ) {
				DEBUG_PRINTLN("Begin ");
				t.setTimeout(500);
				t.begin(baud, config);
				stream = &t;
			}

			template< typename T >
			void begin( T &t, uint32_t baud, uint32_t config, int8_t txE220pin, int8_t rxE220pin ) {
				DEBUG_PRINTLN("Begin ");
				t.setTimeout(500);
				t.begin(baud, config, txE220pin, rxE220pin);
				stream = &t;
			}
#endif

			void listen() {}

			Stream *stream;
		};
		NeedsStream serialDef;

		MODE_TYPE mode = MODE_0_NORMAL;

		void managedDelay(unsigned long timeout);
		Status waitCompleteResponse(unsigned long timeout = 1000, unsigned int waitNoAux = 100);
		void flush();
		void cleanUARTBuffer();

		Status sendStruct(void *structureManaged, uint16_t size_);
		Status receiveStruct(void *structureManaged, uint16_t size_);
		bool writeProgramCommand(PROGRAM_COMMAND cmd, REGISTER_ADDRESS addr, PACKET_LENGHT pl);

		RESPONSE_STATUS checkUARTConfiguration(MODE_TYPE mode);

#ifdef LoRa_E220_DEBUG
		void printParameters(struct Configuration *configuration);
#endif
};

#endif
