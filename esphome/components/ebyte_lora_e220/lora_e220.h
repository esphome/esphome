#pragma once
#include <utility>
#include <string>
#include <vector>
#include "state_naming.h"
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
namespace esphome {
namespace lora_e220 {

#define MAX_SIZE_TX_PACKET 200

static const char *const TAG = "ebyte_lora_e220";
enum MODE_TYPE {
  MODE_0_NORMAL = 0,
  MODE_0_TRANSMISSION = 0,
  MODE_1_WOR_TRANSMITTER = 1,
  MODE_1_WOR = 1,
  MODE_2_WOR_RECEIVER = 2,
  MODE_2_POWER_SAVING = 2,
  MODE_3_CONFIGURATION = 3,
  MODE_3_PROGRAM = 3,
  MODE_3_SLEEP = 3,
  MODE_INIT = 0xFF
};

enum PROGRAM_COMMAND {
  WRITE_CFG_PWR_DWN_SAVE = 0xC0,
  READ_CONFIGURATION = 0xC1,
  WRITE_CFG_PWR_DWN_LOSE = 0xC2,
  WRONG_FORMAT = 0xFF,
  RETURNED_COMMAND = 0xC1,
  SPECIAL_WIFI_CONF_COMMAND = 0xCF
};

enum REGISTER_ADDRESS {
  REG_ADDRESS_CFG = 0x00,
  REG_ADDRESS_SPED = 0x02,
  REG_ADDRESS_TRANS_MODE = 0x03,
  REG_ADDRESS_CHANNEL = 0x04,
  REG_ADDRESS_OPTION = 0x05,
  REG_ADDRESS_CRYPT = 0x06,
  REG_ADDRESS_PID = 0x08
};

enum PACKET_LENGHT {
  PL_CONFIGURATION = 0x08,

  PL_SPED = 0x01,
  PL_OPTION = 0x01,
  PL_TRANSMISSION_MODE = 0x01,
  PL_CHANNEL = 0x01,
  PL_CRYPT = 0x02,
  PL_PID = 0x03
};

struct Speed {
  uint8_t airDataRate : 3;  // bit 0-2
  std::string getAirDataRateDescription() { return state_naming::getAirDataRateDescriptionByParams(this->airDataRate); }

  uint8_t uartParity : 2;  // bit 3-4
  std::string getUARTParityDescription() { return state_naming::getUARTParityDescriptionByParams(this->uartParity); }

  uint8_t uartBaudRate : 3;  // bit 5-7
  std::string getUARTBaudRateDescription() {
    return state_naming::getUARTBaudRateDescriptionByParams(this->uartBaudRate);
  }
};

struct TransmissionMode {
  uint8_t WORPeriod : 3;  // bit 2,1,0
  std::string getWORPeriodByParamsDescription() { return state_naming::getWORPeriodByParams(this->WORPeriod); }
  uint8_t reserved2 : 1;  // bit 3
  uint8_t enableLBT : 1;  // bit 4
  std::string getLBTEnableByteDescription() { return state_naming::getLBTEnableByteByParams(this->enableLBT); }
  uint8_t reserved : 1;  // bit 5

  uint8_t fixedTransmission : 1;  // bit 6
  std::string getFixedTransmissionDescription() {
    return state_naming::getFixedTransmissionDescriptionByParams(this->fixedTransmission);
  }

  uint8_t enableRSSI : 1;  // bit 7
  std::string getRSSIEnableByteDescription() { return state_naming::getRSSIEnableByteByParams(this->enableRSSI); }
};

struct Option {
  uint8_t transmissionPower : 2;  // bit 0-1
  std::string getTransmissionPowerDescription() {
    return state_naming::getTransmissionPowerDescriptionByParams(this->transmissionPower);
  }
  uint8_t reserved : 3;  // bit 2-4

  uint8_t RSSIAmbientNoise : 1;  // bit 5
  std::string getRSSIAmbientNoiseEnable() {
    return state_naming::getRSSIAmbientNoiseEnableByParams(this->RSSIAmbientNoise);
  }

  uint8_t subPacketSetting : 2;  // bit 6-7
  std::string getSubPacketSetting() { return state_naming::getSubPacketSettingByParams(this->subPacketSetting); }
};

struct Crypt {
  uint8_t CRYPT_H = 0;
  uint8_t CRYPT_L = 0;
};

struct Configuration {
  uint8_t COMMAND = 0;
  uint8_t STARTING_ADDRESS = 0;
  uint8_t LENGHT = 0;

  uint8_t ADDH = 0;
  uint8_t ADDL = 0;

  struct Speed SPED;
  struct Option OPTION;

  uint8_t CHAN = 0;
  std::string getChannelDescription() { return std::string(this->CHAN + OPERATING_FREQUENCY + "MHz"); }

  struct TransmissionMode TRANSMISSION_MODE;

  struct Crypt CRYPT;
};

struct ModuleInformation {
  uint8_t COMMAND = 0;
  uint8_t STARTING_ADDRESS = 0;
  uint8_t LENGHT = 0;

  uint8_t model = 0;
  uint8_t version = 0;
  uint8_t features = 0;
};

struct ResponseStatus {
  uint8_t code;
  std::string getResponseDescription() { return state_naming::getResponseDescriptionByParams(this->code); }
};

struct ResponseStructContainer {
  void *data;
  uint8_t rssi;
  ResponseStatus status;
  void close() { free(this->data); }
};
struct ResponseContainer {
  std::string data;
  uint8_t rssi;
  ResponseStatus status;
};

struct ConfigurationMessage {
  uint8_t specialCommand1 = 0xCF;
  uint8_t specialCommand2 = 0xCF;

  unsigned char message[];
};

class LoRa_E220 {
 public:
  LoRa_E220(uart::UARTDevice *device, GPIOPin *auxPin, GPIOPin *m0Pin, GPIOPin *m1Pin);
  bool begin();
  state_naming::Status setMode(MODE_TYPE mode);
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
  ResponseStatus sendFixedMessage(uint8_t ADDH, uint8_t ADDL, uint8_t CHAN, const std::string message);
  ResponseStatus sendFixedMessage(uint8_t ADDH, uint8_t ADDL, uint8_t CHAN, const void *message, const uint8_t size);
  ResponseStatus sendBroadcastFixedMessage(uint8_t CHAN, const void *message, const uint8_t size);
  ResponseStatus sendBroadcastFixedMessage(uint8_t CHAN, const std::string message);
  ResponseContainer receiveInitialMessage(const uint8_t size);
  ResponseStatus sendConfigurationMessage(uint8_t ADDH, uint8_t ADDL, uint8_t CHAN, Configuration *configuration,
                                          PROGRAM_COMMAND programCommand = WRITE_CFG_PWR_DWN_SAVE);
  int available();

 private:
  uart::UARTDevice *serial;
  GPIOPin *auxPin;
  GPIOPin *m0Pin;
  GPIOPin *m1Pin;

  unsigned long halfKeyloqKey = 0x06660708;
  unsigned long encrypt(unsigned long data);
  unsigned long decrypt(unsigned long data);

  MODE_TYPE mode = MODE_0_NORMAL;

  void managedDelay(unsigned long timeout);
  state_naming::Status waitCompleteResponse(unsigned long timeout = 1000, unsigned int waitNoAux = 100);
  void flush();
  void cleanUARTBuffer();

  state_naming::Status sendStruct(void *structureManaged, uint16_t size_);
  state_naming::Status receiveStruct(void *structureManaged, uint16_t size_);
  bool writeProgramCommand(PROGRAM_COMMAND cmd, REGISTER_ADDRESS addr, PACKET_LENGHT pl);

  state_naming::RESPONSE_STATUS checkUARTConfiguration(MODE_TYPE mode);
};
}  // namespace lora_e220
}  // namespace esphome
