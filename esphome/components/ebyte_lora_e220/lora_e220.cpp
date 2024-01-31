#include "lora_e220.h"
namespace esphome {
namespace lora_e220 {
LoRa_E220::LoRa_E220(esphome::uart::UARTDevice *serial, GPIOPin *auxPin, GPIOPin *m0Pin,
                     GPIOPin *m1Pin) {  //, uint32_t serialConfig
  this->serial = serial;
  this->auxPin = auxPin;

  this->m0Pin = m0Pin;
  this->m1Pin = m1Pin;
}
bool LoRa_E220::begin() {
  ESP_LOGD(TAG, "AUX ---> ");
  LOG_PIN(TAG, this->auxPin);
  ESP_LOGD(TAG, "M0 ---> ");
  LOG_PIN(TAG, this->m0Pin);
  ESP_LOGD(TAG, "M1 ---> ");
  LOG_PIN(TAG, this->m1Pin);

  if (this->auxPin != nullptr) {
    this->m0Pin->pin_mode(gpio::FLAG_INPUT);
    ESP_LOGD(TAG, "Init AUX pin!");
  }
  if (this->m0Pin != nullptr) {
    this->m0Pin->pin_mode(gpio::FLAG_OUTPUT);
    ESP_LOGD(TAG, "Init M0 pin!");
    this->m0Pin->digital_write(true);
  }
  if (this->m1Pin != nullptr) {
    this->m0Pin->pin_mode(gpio::FLAG_OUTPUT);
    ESP_LOGD(TAG, "Init M1 pin!");
    this->m1Pin->digital_write(true);
  }

  ESP_LOGD(TAG, "Begin ex");

  state_naming::Status status = setMode(MODE_0_NORMAL);
  return status == state_naming::E220_SUCCESS;
}

/*

Utility method to wait until module is doen tranmitting
a timeout is provided to avoid an infinite loop

*/

state_naming::Status LoRa_E220::waitCompleteResponse(unsigned long timeout, unsigned int waitNoAux) {
  state_naming::Status result = state_naming::E220_SUCCESS;

  unsigned long t = millis();

  // make darn sure millis() is not about to reach max data type limit and start over
  if (((unsigned long) (t + timeout)) == 0) {
    t = 0;
  }

  // if AUX pin was supplied and look for HIGH state
  // note you can omit using AUX if no pins are available, but you will have to use delay() to let module finish
  if (this->auxPin != -1) {
    while (this->auxPin->digital_read() == false) {
      if ((millis() - t) > timeout) {
        result = ERR_E220_TIMEOUT;
        ESP_LOGD(TAG, "Timeout error!");
        return result;
      }
    }
    ESP_LOGD(TAG, "AUX HIGH!");
  } else {
    // if you can't use aux pin, use 4K7 pullup with Arduino
    // you may need to adjust this value if transmissions fail
    this->managedDelay(waitNoAux);
    ESP_LOGD(TAG, "Wait no AUX pin!");
  }

  // per data sheet control after aux goes high is 2ms so delay for at least that long)
  this->managedDelay(20);
  ESP_LOGD(TAG, "Complete!");
  return result;
}

/*

delay() in a library is not a good idea as it can stop interrupts
just poll internal time until timeout is reached

*/

void LoRa_E220::managedDelay(unsigned long timeout) {
  unsigned long t = millis();

  // make darn sure millis() is not about to reach max data type limit and start over
  if (((unsigned long) (t + timeout)) == 0) {
    t = 0;
  }

  while ((millis() - t) < timeout) {
  }
}

/*

Method to indicate availability

*/

int LoRa_E220::available() { return this->serial->available(); }

void LoRa_E220::flush() { this->serial->flush(); }

void LoRa_E220::cleanUARTBuffer() {
  while (this->serial->available()) {
    this->serial->read();
  }
}

/*

Method to send a chunk of data provided data is in a struct--my personal favorite as you
need not parse or worry about sprintf() inability to handle floats

TTP: put your structure definition into a .h file and include in both the sender and reciever
sketches

NOTE: of your sender and receiver MCU's are different (Teensy and Arduino) caution on the data
types each handle ints floats differently

*/

state_naming::Status LoRa_E220::sendStruct(void *structureManaged, uint16_t size_) {
  if (size_ > MAX_SIZE_TX_PACKET + 2) {
    return ERR_E220_PACKET_TOO_BIG;
  }

  Status result = E220_SUCCESS;

  uint8_t len = this->serial->write((uint8_t *) structureManaged, size_);
  if (len != size_) {
    ESP_LOGD(TAG, "Send... len:")
    ESP_LOGD(TAG, "%d", len);
    ESP_LOGD(TAG, " size:")
    ESP_LOGD(TAG, "%d", size_);
    if (len == 0) {
      result = state_naming::ERR_E220_NO_RESPONSE_FROM_DEVICE;
    } else {
      result = state_naming::ERR_E220_DATA_SIZE_NOT_MATCH;
    }
  }
  if (result != state_naming::E220_SUCCESS)
    return result;

  result = this->waitCompleteResponse(5000, 5000);
  if (result != state_naming::E220_SUCCESS)
    return result;
  ESP_LOGD(TAG, "Clear buffer...")
  this->cleanUARTBuffer();

  ESP_LOGD(TAG, "ok!")

  return result;
}

/*

Method to get a chunk of data provided data is in a struct--my personal favorite as you
need not parse or worry about sprintf() inability to handle floats

TTP: put your structure definition into a .h file and include in both the sender and reciever
sketches

NOTE: of your sender and receiver MCU's are different (Teensy and Arduino) caution on the data
types each handle ints floats differently

*/

state_naming::Status LoRa_E220::receiveStruct(void *structureManaged, uint16_t size_) {
  Status result = E220_SUCCESS;

  uint8_t len = this->serialDef.stream->readBytes((uint8_t *) structureManaged, size_);

  ESP_LOGD(TAG, "Available buffer: ");
  ESP_LOGD(TAG, "%d", len);
  ESP_LOGD(TAG, " structure size: ");
  ESP_LOGD(TAG, "%d", size_);

  if (len != size_) {
    if (len == 0) {
      result = state_naming::ERR_E220_NO_RESPONSE_FROM_DEVICE;
    } else {
      result = state_naming::ERR_E220_DATA_SIZE_NOT_MATCH;
    }
  }
  if (result != state_naming::E220_SUCCESS)
    return result;

  result = this->waitCompleteResponse(1000);
  if (result != state_naming::E220_SUCCESS)
    return result;

  return result;
}

/*

method to set the mode (program, normal, etc.)

*/

state_naming::Status LoRa_E220::setMode(MODE_TYPE mode) {
  // data sheet claims module needs some extra time after mode setting (2ms)
  // most of my projects uses 10 ms, but 40ms is safer

  this->managedDelay(40);

  if (this->m0Pin == -1 && this->m1Pin == -1) {
    ESP_LOGD(TAG, "The M0 and M1 pins is not set, this mean that you are connect directly the pins as you need!")
  } else {
    switch (mode) {
      case MODE_0_NORMAL:
        // Mode 0 | normal operation
        this->m0Pin->digital_write(false);
        this->m1Pin->digital_write(false);
        ESP_LOGD(TAG, "MODE NORMAL!");
        break;
      case MODE_1_WOR_TRANSMITTER:
        this->m0Pin->digital_write(true);
        this->m1Pin->digital_write(false);
        ESP_LOGD(TAG, "MODE WOR!");
        break;
      case MODE_2_WOR_RECEIVER:
        //		  case MODE_2_PROGRAM:
        this->m0Pin->digital_write(false);
        this->m1Pin->digital_write(true);
        ESP_LOGD(TAG, "MODE RECEIVING!");
        break;
      case MODE_3_CONFIGURATION:
        // Mode 3 | Setting operation
        this->m0Pin->digital_write(true);
        this->m1Pin->digital_write(true);
        ESP_LOGD(TAG, "MODE SLEEP CONFIG!");
        break;

      default:
        return state_naming::ERR_E220_INVALID_PARAM;
    }
  }
  // data sheet says 2ms later control is returned, let's give just a bit more time
  // these modules can take time to activate pins
  this->managedDelay(40);

  // wait until aux pin goes back low
  state_naming::Status res = this->waitCompleteResponse(1000);

  if (res == state_naming::E220_SUCCESS) {
    this->mode = mode;
  }

  return res;
}

MODE_TYPE LoRa_E220::getMode() { return this->mode; }

bool LoRa_E220::writeProgramCommand(PROGRAM_COMMAND cmd, REGISTER_ADDRESS addr, PACKET_LENGHT pl) {
  uint8_t CMD[3] = {cmd, addr, pl};
  uint8_t size = this->serial->write_array(CMD, 3);

  ESP_LOGD(TAG, "%d", size);

  this->managedDelay(50);  // need ti check

  return size != 2;
}

ResponseStructContainer LoRa_E220::getConfiguration() {
  ResponseStructContainer rc;

  rc.status.code = checkUARTConfiguration(MODE_3_PROGRAM);
  if (rc.status.code != state_naming::E220_SUCCESS)
    return rc;

  MODE_TYPE prevMode = this->mode;

  rc.status.code = this->setMode(MODE_3_PROGRAM);
  if (rc.status.code != E220_SUCCESS)
    return rc;

  this->writeProgramCommand(READ_CONFIGURATION, REG_ADDRESS_CFG, PL_CONFIGURATION);

  rc.data = malloc(sizeof(Configuration));
  rc.status.code = this->receiveStruct((uint8_t *) rc.data, sizeof(Configuration));

#ifdef LoRa_E220_DEBUG
  this->printParameters((Configuration *) rc.data);
#endif

  if (rc.status.code != E220_SUCCESS) {
    this->setMode(prevMode);
    return rc;
  }

  rc.status.code = this->setMode(prevMode);
  if (rc.status.code != E220_SUCCESS)
    return rc;

  if (WRONG_FORMAT == ((Configuration *) rc.data)->COMMAND) {
    rc.status.code = ERR_E220_WRONG_FORMAT;
  }
  if (RETURNED_COMMAND != ((Configuration *) rc.data)->COMMAND ||
      REG_ADDRESS_CFG != ((Configuration *) rc.data)->STARTING_ADDRESS ||
      PL_CONFIGURATION != ((Configuration *) rc.data)->LENGHT) {
    rc.status.code = ERR_E220_HEAD_NOT_RECOGNIZED;
  }

  return rc;
}

state_naming::RESPONSE_STATUS LoRa_E220::checkUARTConfiguration(MODE_TYPE mode) {
  if (mode == MODE_3_PROGRAM) {
    return ERR_E220_WRONG_UART_CONFIG;
  }
  return E220_SUCCESS;
}

ResponseStatus LoRa_E220::setConfiguration(Configuration configuration, PROGRAM_COMMAND saveType) {
  ResponseStatus rc;

  rc.code = checkUARTConfiguration(MODE_3_PROGRAM);
  if (rc.code != E220_SUCCESS)
    return rc;

  MODE_TYPE prevMode = this->mode;

  rc.code = this->setMode(MODE_3_PROGRAM);
  if (rc.code != E220_SUCCESS)
    return rc;

  //	this->writeProgramCommand(saveType, REG_ADDRESS_CFG);

  //	configuration.HEAD = saveType;
  configuration.COMMAND = saveType;
  configuration.STARTING_ADDRESS = REG_ADDRESS_CFG;
  configuration.LENGHT = PL_CONFIGURATION;

  rc.code = this->sendStruct((uint8_t *) &configuration, sizeof(Configuration));
  if (rc.code != E220_SUCCESS) {
    this->setMode(prevMode);
    return rc;
  }

  rc.code = this->receiveStruct((uint8_t *) &configuration, sizeof(Configuration));

#ifdef LoRa_E220_DEBUG
  this->printParameters((Configuration *) &configuration);
#endif

  rc.code = this->setMode(prevMode);
  if (rc.code != E220_SUCCESS)
    return rc;

  if (WRONG_FORMAT == ((Configuration *) &configuration)->COMMAND) {
    rc.code = ERR_E220_WRONG_FORMAT;
  }
  if (RETURNED_COMMAND != ((Configuration *) &configuration)->COMMAND ||
      REG_ADDRESS_CFG != ((Configuration *) &configuration)->STARTING_ADDRESS ||
      PL_CONFIGURATION != ((Configuration *) &configuration)->LENGHT) {
    rc.code = ERR_E220_HEAD_NOT_RECOGNIZED;
  }

  return rc;
}

ResponseStructContainer LoRa_E220::getModuleInformation() {
  ResponseStructContainer rc;

  rc.status.code = checkUARTConfiguration(MODE_3_PROGRAM);
  if (rc.status.code != E220_SUCCESS)
    return rc;

  MODE_TYPE prevMode = this->mode;

  rc.status.code = this->setMode(MODE_3_PROGRAM);
  if (rc.status.code != E220_SUCCESS)
    return rc;

  this->writeProgramCommand(READ_CONFIGURATION, REG_ADDRESS_PID, PL_PID);

  rc.data = malloc(sizeof(ModuleInformation));

  rc.status.code = this->receiveStruct((uint8_t *) rc.data, sizeof(ModuleInformation));
  if (rc.status.code != E220_SUCCESS) {
    this->setMode(prevMode);
    return rc;
  }

  rc.status.code = this->setMode(prevMode);
  if (rc.status.code != E220_SUCCESS)
    return rc;

  //	this->printParameters(*configuration);

  if (WRONG_FORMAT == ((ModuleInformation *) rc.data)->COMMAND) {
    rc.status.code = ERR_E220_WRONG_FORMAT;
  }
  if (RETURNED_COMMAND != ((ModuleInformation *) rc.data)->COMMAND ||
      REG_ADDRESS_PID != ((ModuleInformation *) rc.data)->STARTING_ADDRESS ||
      PL_PID != ((ModuleInformation *) rc.data)->LENGHT) {
    rc.status.code = ERR_E220_HEAD_NOT_RECOGNIZED;
  }

  ESP_LOGD(TAG, "----------------------------------------");
  ESP_LOGD(TAG, "HEAD: ");
  ESP_LOGD(TAG, "%d", ((ModuleInformation *) rc.data)->COMMAND);
  ESP_LOGD(TAG, " ");
  ESP_LOGD(TAG, "%d", ((ModuleInformation *) rc.data)->STARTING_ADDRESS);
  ESP_LOGD(TAG, " ");
  ESP_LOGD(TAG, "%s", format_hex_pretty(((ModuleInformation *) rc.data)->LENGHT).c_str());

  ESP_LOGD(TAG, "Model no.: ");
  ESP_LOGD(TAG, "%s", format_hex_pretty(((ModuleInformation *) rc.data)->model).c_str());
  ESP_LOGD(TAG, "Version  : ");
  ESP_LOGD(TAG, "%s", format_hex_pretty(((ModuleInformation *) rc.data)->version).c_str());
  ESP_LOGD(TAG, "Features : ");
  ESP_LOGD(TAG, "%s", format_hex_pretty(((ModuleInformation *) rc.data)->features).c_str());
  ESP_LOGD(TAG, "Status : ");
  ESP_LOGD(TAG, rc.status.getResponseDescription());
  ESP_LOGD(TAG, "----------------------------------------");
  return rc;
}

ResponseStatus LoRa_E220::resetModule() {
  ESP_LOGD(TAG, "No information to reset module!");
  ResponseStatus status;
  status.code = ERR_E220_NOT_IMPLEMENT;
  return status;
}

state_naming::ResponseContainer LoRa_E220::receiveMessage() { return LoRa_E220::receiveMessageComplete(false); }
state_naming::ResponseContainer LoRa_E220::receiveMessageRSSI() { return LoRa_E220::receiveMessageComplete(true); }

state_naming::ResponseContainer LoRa_E220::receiveMessageComplete(bool rssiEnabled) {
  ResponseContainer rc;
  rc.status.code = E220_SUCCESS;
  std::string buffer;
  uint8_t data;
  while (this->available() > 0) {
    if (this->read_byte(&data)) {
      buffer += (char) data;
    }
  }
  ESP_LOGD(TAG, buffer);

  if (rssiEnabled) {
    rc.rssi = buffer.charAt(tmpData.length() - 1);
    rc.data = buffer.substd::string(0, tmpData.length() - 1);
  } else {
    rc.data = tmpData;
  }
  this->cleanUARTBuffer();
  if (rc.status.code != E220_SUCCESS) {
    return rc;
  }

  //	rc.data = message; // malloc(sizeof (moduleInformation));

  return rc;
}

state_naming::ResponseContainer LoRa_E220::receiveInitialMessage(uint8_t size) {
  ResponseContainer rc;
  rc.status.code = E220_SUCCESS;
  char buff[size];
  uint8_t len = this->serialDef.stream->readBytes(buff, size);
  if (len != size) {
    if (len == 0) {
      rc.status.code = ERR_E220_NO_RESPONSE_FROM_DEVICE;
    } else {
      rc.status.code = ERR_E220_DATA_SIZE_NOT_MATCH;
    }
    return rc;
  }

  rc.data = buff;  // malloc(sizeof (moduleInformation));

  return rc;
}

state_naming::ResponseStructContainer LoRa_E220::receiveMessage(const uint8_t size) {
  return LoRa_E220::receiveMessageComplete(size, false);
}
state_naming::ResponseStructContainer LoRa_E220::receiveMessageRSSI(const uint8_t size) {
  return LoRa_E220::receiveMessageComplete(size, true);
}

state_naming::ResponseStructContainer LoRa_E220::receiveMessageComplete(const uint8_t size, bool rssiEnabled) {
  ResponseStructContainer rc;

  rc.data = malloc(size);
  rc.status.code = this->receiveStruct((uint8_t *) rc.data, size);
  if (rc.status.code != E220_SUCCESS) {
    return rc;
  }

  if (rssiEnabled) {
    char rssi[1];
    this->serialDef.stream->readBytes(rssi, 1);
    rc.rssi = rssi[0];
  }
  this->cleanUARTBuffer();

  return rc;
}

ResponseStatus LoRa_E220::sendMessage(const void *message, const uint8_t size) {
  ResponseStatus status;
  status.code = this->sendStruct((uint8_t *) message, size);
  if (status.code != E220_SUCCESS)
    return status;

  return status;
}
ResponseStatus LoRa_E220::sendMessage(const std::string message) {
  ESP_LOGD(TAG, "Send message: ");
  ESP_LOGD(TAG, message);
  byte size = message.length();  // sizeof(message.c_str())+1;
  ESP_LOGD(TAG, " size: ");
  ESP_LOGD(TAG, "%d", size);
  char messageFixed[size];
  memcpy(messageFixed, message.c_str(), size);
  ESP_LOGD(TAG, " memcpy ");

  ResponseStatus status;
  status.code = this->sendStruct((uint8_t *) &messageFixed, size);
  if (status.code != E220_SUCCESS)
    return status;

  //	free(messageFixed);
  return status;
}

ResponseStatus LoRa_E220::sendFixedMessage(byte ADDH, byte ADDL, byte CHAN, const std::string message) {
  byte size = message.length();
  char messageFixed[size];
  memcpy(messageFixed, message.c_str(), size);
  return this->sendFixedMessage(ADDH, ADDL, CHAN, (uint8_t *) messageFixed, size);
}
ResponseStatus LoRa_E220::sendBroadcastFixedMessage(byte CHAN, const std::string message) {
  return this->sendFixedMessage(BROADCAST_ADDRESS, BROADCAST_ADDRESS, CHAN, message);
}

typedef struct fixedStransmission {
  byte ADDH = 0;
  byte ADDL = 0;
  byte CHAN = 0;
  unsigned char message[];
} FixedStransmission;

FixedStransmission *init_stack(int m) {
  FixedStransmission *st = (FixedStransmission *) malloc(sizeof(FixedStransmission) + m * sizeof(int));
  return st;
}

ResponseStatus LoRa_E220::sendFixedMessage(byte ADDH, byte ADDL, byte CHAN, const void *message, const uint8_t size) {
  ESP_LOGD(TAG, ADDH);
  FixedStransmission *fixedStransmission = init_stack(size);
  fixedStransmission->ADDH = ADDH;
  fixedStransmission->ADDL = ADDL;
  fixedStransmission->CHAN = CHAN;
  memcpy(fixedStransmission->message, (unsigned char *) message, size);
  ResponseStatus status;
  status.code = this->sendStruct((uint8_t *) fixedStransmission, size + 3);
  free(fixedStransmission);
  if (status.code != E220_SUCCESS)
    return status;

  return status;
}

ConfigurationMessage *init_stack_conf(int m) {
  ConfigurationMessage *st = (ConfigurationMessage *) malloc(sizeof(ConfigurationMessage) + m * sizeof(int));
  return st;
}

ResponseStatus LoRa_E220::sendConfigurationMessage(byte ADDH, byte ADDL, byte CHAN, Configuration *configuration,
                                                   PROGRAM_COMMAND programCommand) {
  ResponseStatus rc;

  configuration->COMMAND = programCommand;
  configuration->STARTING_ADDRESS = REG_ADDRESS_CFG;
  configuration->LENGHT = PL_CONFIGURATION;

  ConfigurationMessage *fixedStransmission = init_stack_conf(sizeof(Configuration));

  memcpy(fixedStransmission->message, (unsigned char *) configuration, sizeof(Configuration));

  fixedStransmission->specialCommand1 = SPECIAL_WIFI_CONF_COMMAND;
  fixedStransmission->specialCommand2 = SPECIAL_WIFI_CONF_COMMAND;

  ESP_LOGD(TAG, "%d", sizeof(Configuration) + 2);

  rc = sendFixedMessage(ADDH, ADDL, CHAN, fixedStransmission, sizeof(Configuration) + 2);
  return rc;
}

ResponseStatus LoRa_E220::sendBroadcastFixedMessage(byte CHAN, const void *message, const uint8_t size) {
  return this->sendFixedMessage(0xFF, 0xFF, CHAN, message, size);
}

#define KeeLoq_NLF 0x3A5C742E

unsigned long LoRa_E220::encrypt(unsigned long data) {
  unsigned long x = data;
  unsigned long r;
  int keyBitNo, index;
  unsigned long keyBitVal, bitVal;

  for (r = 0; r < 528; r++) {
    keyBitNo = r & 63;
    if (keyBitNo < 32)
      keyBitVal = bitRead(this->halfKeyloqKey, keyBitNo);  // key low
    else
      keyBitVal = bitRead(this->halfKeyloqKey, keyBitNo - 32);  // key hight
    index = 1 * bitRead(x, 1) + 2 * bitRead(x, 9) + 4 * bitRead(x, 20) + 8 * bitRead(x, 26) + 16 * bitRead(x, 31);
    bitVal = bitRead(x, 0) ^ bitRead(x, 16) ^ bitRead(KeeLoq_NLF, index) ^ keyBitVal;
    x = (x >> 1) ^ bitVal << 31;
  }
  return x;
}

unsigned long LoRa_E220::decrypt(unsigned long data) {
  unsigned long x = data;
  unsigned long r;
  int keyBitNo, index;
  unsigned long keyBitVal, bitVal;

  for (r = 0; r < 528; r++) {
    keyBitNo = (15 - r) & 63;
    if (keyBitNo < 32)
      keyBitVal = bitRead(this->halfKeyloqKey, keyBitNo);  // key low
    else
      keyBitVal = bitRead(this->halfKeyloqKey, keyBitNo - 32);  // key hight
    index = 1 * bitRead(x, 0) + 2 * bitRead(x, 8) + 4 * bitRead(x, 19) + 8 * bitRead(x, 25) + 16 * bitRead(x, 30);
    bitVal = bitRead(x, 31) ^ bitRead(x, 15) ^ bitRead(KeeLoq_NLF, index) ^ keyBitVal;
    x = (x << 1) ^ bitVal;
  }
  return x;
}
#ifdef LoRa_E220_DEBUG
void LoRa_E220::printParameters(struct Configuration *configuration) {
  ESP_LOGD(TAG, "----------------------------------------");

  ESP_LOGD(TAG, "HEAD : ");
  ESP_LOGD(TAG, "%s", format_hex_pretty(configuration->COMMAND).c_str());
  ESP_LOGD(TAG, " ");
  ESP_LOGD(TAG, format_hex_pretty(configuration->STARTING_ADDRESS).c_str());
  ESP_LOGD(TAG, " ");
  ESP_LOGD(TAG, format_hex_pretty(configuration->LENGHT).c_str());
  ESP_LOGD(TAG, " ");
  ESP_LOGD(TAG, "AddH : ");
  ESP_LOGD(TAG,  format_hex_pretty(configuration->ADDH).c_str();
  ESP_LOGD(TAG, "AddL : ");
  ESP_LOGD(TAG,  format_hex_pretty(configuration->ADDL).c_str());
  ESP_LOGD(TAG, " ");
  ESP_LOGD(TAG, "Chan : ");
  ESP_LOGD(TAG, "%d", configuration->CHAN);
  ESP_LOGD(TAG, " -> ");
  ESP_LOGD(TAG, configuration->getChannelDescription());
  ESP_LOGD(TAG, " ");
  ESP_LOGD(TAG, "SpeedParityBit     : ");
  ESP_LOGD(TAG, "%d", configuration->SPED.uartParity);
  ESP_LOGD(TAG, " -> ");
  ESP_LOGD(TAG, configuration->SPED.getUARTParityDescription());
  ESP_LOGD(TAG, "SpeedUARTDatte     : ");
  ESP_LOGD(TAG,"%d",  configuration->SPED.uartBaudRate);
  ESP_LOGD(TAG, " -> ");
  ESP_LOGD(TAG, configuration->SPED.getUARTBaudRateDescription());
  ESP_LOGD(TAG, "SpeedAirDataRate   : ");
  ESP_LOGD(TAG,"%d",  configuration->SPED.airDataRate);
  ESP_LOGD(TAG, " -> ");
  ESP_LOGD(TAG, configuration->SPED.getAirDataRateDescription());
  ESP_LOGD(TAG, " ");
  ESP_LOGD(TAG, "OptionSubPacketSett: ");
  ESP_LOGD(TAG,"%d",  configuration->OPTION.subPacketSetting);
  ESP_LOGD(TAG, " -> ");
  ESP_LOGD(TAG, configuration->OPTION.getSubPacketSetting());
  ESP_LOGD(TAG, "OptionTranPower    : ");
  ESP_LOGD(TAG, configuration->OPTION.transmissionPower);
  ESP_LOGD(TAG, " -> ");
  ESP_LOGD(TAG, configuration->OPTION.getTransmissionPowerDescription());
  ESP_LOGD(TAG, "OptionRSSIAmbientNo: ");
  ESP_LOGD(TAG,"%d",  configuration->OPTION.RSSIAmbientNoise);
  ESP_LOGD(TAG, " -> ");
  ESP_LOGD(TAG, configuration->OPTION.getRSSIAmbientNoiseEnable());
  ESP_LOGD(TAG, " ");
  ESP_LOGD(TAG, "TransModeWORPeriod : ");
  ESP_LOGD(TAG,"%d",  configuration->TRANSMISSION_MODE.WORPeriod);
  ESP_LOGD(TAG, " -> ");
  ESP_LOGD(TAG, configuration->TRANSMISSION_MODE.getWORPeriodByParamsDescription());
  ESP_LOGD(TAG, "TransModeEnableLBT : ");
  ESP_LOGD(TAG,"%d",  configuration->TRANSMISSION_MODE.enableLBT);
  ESP_LOGD(TAG, " -> ");
  ESP_LOGD(TAG, configuration->TRANSMISSION_MODE.getLBTEnableByteDescription());
  ESP_LOGD(TAG, "TransModeEnableRSSI: ");
  ESP_LOGD(TAG, "%d", configuration->TRANSMISSION_MODE.enableRSSI);
  ESP_LOGD(TAG, " -> ");
  ESP_LOGD(TAG, configuration->TRANSMISSION_MODE.getRSSIEnableByteDescription());
  ESP_LOGD(TAG, "TransModeFixedTrans: ");
  ESP_LOGD(TAG, "%d", configuration->TRANSMISSION_MODE.fixedTransmission);
  ESP_LOGD(TAG, " -> ");
  ESP_LOGD(TAG, configuration->TRANSMISSION_MODE.getFixedTransmissionDescription());

  ESP_LOGD(TAG, "----------------------------------------");
}
#endif
}  // namespace lora_e220
}  // namespace esphome
