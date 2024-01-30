#include "lora_e220.h"

#ifdef ACTIVATE_SOFTWARE_SERIAL
LoRa_E220::LoRa_E220(byte txE220pin, byte rxE220pin, UART_BPS_RATE bpsRate){
    this->txE220pin = txE220pin;
    this->rxE220pin = rxE220pin;
    SoftwareSerial* mySerial = new SoftwareSerial((uint8_t)this->txE220pin, (uint8_t)this->rxE220pin); // "RX TX" // @suppress("Abstract class cannot be instantiated")
    this->ss = mySerial;
    this->hs = NULL;

    this->bpsRate = bpsRate;
}
LoRa_E220::LoRa_E220(byte txE220pin, byte rxE220pin, byte auxPin, UART_BPS_RATE bpsRate){
    this->txE220pin = txE220pin;
    this->rxE220pin = rxE220pin;
    this->auxPin = auxPin;
    SoftwareSerial* mySerial = new SoftwareSerial((uint8_t)this->txE220pin, (uint8_t)this->rxE220pin); // "RX TX" // @suppress("Abstract class cannot be instantiated")
    this->ss = mySerial;
    this->hs = NULL;

    this->bpsRate = bpsRate;
}
LoRa_E220::LoRa_E220(byte txE220pin, byte rxE220pin, byte auxPin, byte m0Pin, byte m1Pin, UART_BPS_RATE bpsRate){
    this->txE220pin = txE220pin;
    this->rxE220pin = rxE220pin;

    this->auxPin = auxPin;

    this->m0Pin = m0Pin;
    this->m1Pin = m1Pin;

    SoftwareSerial* mySerial = new SoftwareSerial((uint8_t)this->txE220pin, (uint8_t)this->rxE220pin); // "RX TX" // @suppress("Abstract class cannot be instantiated")
    this->ss = mySerial;
    this->hs = NULL;

    this->bpsRate = bpsRate;
}
#endif

LoRa_E220::LoRa_E220(HardwareSerial* serial, UART_BPS_RATE bpsRate){ //, uint32_t serialConfig
    this->txE220pin = txE220pin;
    this->rxE220pin = rxE220pin;

	#ifdef ACTIVATE_SOFTWARE_SERIAL
    	this->ss = NULL;
	#endif

    this->hs = serial;

//    this->serialConfig = serialConfig;

    this->bpsRate = bpsRate;
}
LoRa_E220::LoRa_E220(HardwareSerial* serial, byte auxPin, UART_BPS_RATE bpsRate){ // , uint32_t serialConfig
    this->txE220pin = txE220pin;
    this->rxE220pin = rxE220pin;
    this->auxPin = auxPin;

	#ifdef ACTIVATE_SOFTWARE_SERIAL
		this->ss = NULL;
	#endif

	this->hs = serial;

//    this->serialConfig = serialConfig;

    this->bpsRate = bpsRate;
}
LoRa_E220::LoRa_E220(HardwareSerial* serial, byte auxPin, byte m0Pin, byte m1Pin, UART_BPS_RATE bpsRate){ //, uint32_t serialConfig
    this->txE220pin = txE220pin;
    this->rxE220pin = rxE220pin;

    this->auxPin = auxPin;

    this->m0Pin = m0Pin;
    this->m1Pin = m1Pin;

	#ifdef ACTIVATE_SOFTWARE_SERIAL
		this->ss = NULL;
	#endif

    this->hs = serial;
//    this->serialConfig = serialConfig;

    this->bpsRate = bpsRate;
}

#ifdef HARDWARE_SERIAL_SELECTABLE_PIN
LoRa_E220::LoRa_E220(byte txE220pin, byte rxE220pin, HardwareSerial* serial, UART_BPS_RATE bpsRate, uint32_t serialConfig){
    this->txE220pin = txE220pin;
    this->rxE220pin = rxE220pin;

	#ifdef ACTIVATE_SOFTWARE_SERIAL
    	this->ss = NULL;
	#endif

    this->serialConfig = serialConfig;

    this->hs = serial;

    this->bpsRate = bpsRate;
}
LoRa_E220::LoRa_E220(byte txE220pin, byte rxE220pin, HardwareSerial* serial, byte auxPin, UART_BPS_RATE bpsRate, uint32_t serialConfig){
    this->txE220pin = txE220pin;
    this->rxE220pin = rxE220pin;
    this->auxPin = auxPin;

	#ifdef ACTIVATE_SOFTWARE_SERIAL
		this->ss = NULL;
	#endif

	this->serialConfig = serialConfig;

	this->hs = serial;

    this->bpsRate = bpsRate;
}
LoRa_E220::LoRa_E220(byte txE220pin, byte rxE220pin, HardwareSerial* serial, byte auxPin, byte m0Pin, byte m1Pin, UART_BPS_RATE bpsRate, uint32_t serialConfig){
    this->txE220pin = txE220pin;
    this->rxE220pin = rxE220pin;

    this->auxPin = auxPin;

    this->m0Pin = m0Pin;
    this->m1Pin = m1Pin;

	#ifdef ACTIVATE_SOFTWARE_SERIAL
		this->ss = NULL;
	#endif

	this->serialConfig = serialConfig;

    this->hs = serial;

    this->bpsRate = bpsRate;
}
#endif

#ifdef ACTIVATE_SOFTWARE_SERIAL

LoRa_E220::LoRa_E220(SoftwareSerial* serial, UART_BPS_RATE bpsRate){
    this->txE220pin = txE220pin;
    this->rxE220pin = rxE220pin;

    this->ss = serial;
    this->hs = NULL;

    this->bpsRate = bpsRate;
}
LoRa_E220::LoRa_E220(SoftwareSerial* serial, byte auxPin, UART_BPS_RATE bpsRate){
    this->txE220pin = txE220pin;
    this->rxE220pin = rxE220pin;
    this->auxPin = auxPin;

    this->ss = serial;
    this->hs = NULL;

    this->bpsRate = bpsRate;
}
LoRa_E220::LoRa_E220(SoftwareSerial* serial, byte auxPin, byte m0Pin, byte m1Pin, UART_BPS_RATE bpsRate){
    this->txE220pin = txE220pin;
    this->rxE220pin = rxE220pin;

    this->auxPin = auxPin;

    this->m0Pin = m0Pin;
    this->m1Pin = m1Pin;

    this->ss = serial;
    this->hs = NULL;

    this->bpsRate = bpsRate;
}
#endif

bool LoRa_E220::begin(){
	DEBUG_PRINT("RX MIC ---> ");
	DEBUG_PRINTLN(this->txE220pin);
	DEBUG_PRINT("TX MIC ---> ");
	DEBUG_PRINTLN(this->rxE220pin);
	DEBUG_PRINT("AUX ---> ");
	DEBUG_PRINTLN(this->auxPin);
	DEBUG_PRINT("M0 ---> ");
	DEBUG_PRINTLN(this->m0Pin);
	DEBUG_PRINT("M1 ---> ");
	DEBUG_PRINTLN(this->m1Pin);

	if (this->auxPin != -1) {
		pinMode(this->auxPin, INPUT);
		DEBUG_PRINTLN("Init AUX pin!");
	}
	if (this->m0Pin != -1) {
		pinMode(this->m0Pin, OUTPUT);
		DEBUG_PRINTLN("Init M0 pin!");
		digitalWrite(this->m0Pin, HIGH);

	}
	if (this->m1Pin != -1) {
		pinMode(this->m1Pin, OUTPUT);
		DEBUG_PRINTLN("Init M1 pin!");
		digitalWrite(this->m1Pin, HIGH);

	}

    DEBUG_PRINTLN("Begin ex");
    if (this->hs){
        DEBUG_PRINTLN("Begin Hardware Serial");

#ifdef HARDWARE_SERIAL_SELECTABLE_PIN
        if(this->txE220pin != -1 && this->rxE220pin != -1) {
        	DEBUG_PRINTLN("PIN SELECTED!!");
			this->serialDef.begin(*this->hs, this->bpsRate, this->serialConfig, this->txE220pin, this->rxE220pin);
		}else{
			this->serialDef.begin(*this->hs, this->bpsRate, this->serialConfig);
		}
#endif
#ifndef HARDWARE_SERIAL_SELECTABLE_PIN
        this->serialDef.begin(*this->hs, this->bpsRate);
#endif
        while (!this->hs) {
          ; // wait for serial port to connect. Needed for native USB
        }

#ifdef ACTIVATE_SOFTWARE_SERIAL
    }else if (this->ss){
        DEBUG_PRINTLN("Begin Software Serial");

		this->serialDef.begin(*this->ss, this->bpsRate);
	}	else{
        DEBUG_PRINTLN("Begin Software Serial Pin");
        SoftwareSerial* mySerial = new SoftwareSerial((int)this->txE220pin, (int)this->rxE220pin); // "RX TX" // @suppress("Abstract class cannot be instantiated")
        this->ss = mySerial;

//		SoftwareSerial mySerial(this->txE220pin, this->rxE220pin);
        DEBUG_PRINT("RX Pin: ");
        DEBUG_PRINT((int)this->txE220pin);
        DEBUG_PRINT("TX Pin: ");
        DEBUG_PRINTLN((int)this->rxE220pin);

		this->serialDef.begin(*this->ss, this->bpsRate);
#endif
	}

    this->serialDef.stream->setTimeout(100);
    Status status = setMode(MODE_0_NORMAL);
    return status==E220_SUCCESS;
}

/*

Utility method to wait until module is doen tranmitting
a timeout is provided to avoid an infinite loop

*/

Status LoRa_E220::waitCompleteResponse(unsigned long timeout, unsigned int waitNoAux) {

	Status result = E220_SUCCESS;

	unsigned long t = millis();

	// make darn sure millis() is not about to reach max data type limit and start over
	if (((unsigned long) (t + timeout)) == 0){
		t = 0;
	}

	// if AUX pin was supplied and look for HIGH state
	// note you can omit using AUX if no pins are available, but you will have to use delay() to let module finish
	if (this->auxPin != -1) {
		while (digitalRead(this->auxPin) == LOW) {
			if ((millis() - t) > timeout){
				result = ERR_E220_TIMEOUT;
				DEBUG_PRINTLN("Timeout error!");
				return result;
			}
		}
		DEBUG_PRINTLN("AUX HIGH!");
	}
	else {
		// if you can't use aux pin, use 4K7 pullup with Arduino
		// you may need to adjust this value if transmissions fail
		this->managedDelay(waitNoAux);
		DEBUG_PRINTLN(F("Wait no AUX pin!"));
	}


	// per data sheet control after aux goes high is 2ms so delay for at least that long)
	this->managedDelay(20);
	DEBUG_PRINTLN(F("Complete!"));
	return result;
}

/*

delay() in a library is not a good idea as it can stop interrupts
just poll internal time until timeout is reached

*/


void LoRa_E220::managedDelay(unsigned long timeout) {

	unsigned long t = millis();

	// make darn sure millis() is not about to reach max data type limit and start over
	if (((unsigned long) (t + timeout)) == 0){
		t = 0;
	}

	while ((millis() - t) < timeout) 	{ 	}

}

/*

Method to indicate availability

*/

//int LoRa_E220::available(unsigned long timeout) {
int LoRa_E220::available() {
//	unsigned long t = millis();
//
//	// make darn sure millis() is not about to reach max data type limit and start over
//	if (((unsigned long) (t + timeout)) == 0){
//		t = 0;
//	}
//
//	if (this->auxPin != -1) {
//		if (digitalRead(this->auxPin) == HIGH){
//			return 0;
//		}else{
//			while (digitalRead(this->auxPin) == LOW) {
//				if ((millis() - t) > timeout){
//					DEBUG_PRINTLN("Timeout error!");
//					return 0;
//				}
//			}
//			DEBUG_PRINTLN("AUX HIGH!");
//			return 2;
//		}
//	}else{
		return this->serialDef.stream->available();
//	}
}

/*

Method to indicate availability

*/

void LoRa_E220::flush() {
	this->serialDef.stream->flush();
}


void LoRa_E220::cleanUARTBuffer()
{
//  bool IsNull = true;

  while (this->available())
  {
//    IsNull = false;

    this->serialDef.stream->read();
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

Status LoRa_E220::sendStruct(void *structureManaged, uint16_t size_) {
		if (size_ > MAX_SIZE_TX_PACKET + 2){
			return ERR_E220_PACKET_TOO_BIG;
		}

		Status result = E220_SUCCESS;

		uint8_t len = this->serialDef.stream->write((uint8_t *) structureManaged, size_);
		if (len!=size_){
			DEBUG_PRINT(F("Send... len:"))
			DEBUG_PRINT(len);
			DEBUG_PRINT(F(" size:"))
			DEBUG_PRINT(size_);
			if (len==0){
				result = ERR_E220_NO_RESPONSE_FROM_DEVICE;
			}else{
				result = ERR_E220_DATA_SIZE_NOT_MATCH;
			}
		}
		if (result != E220_SUCCESS) return result;

		result = this->waitCompleteResponse(5000, 5000);
		if (result != E220_SUCCESS) return result;
        DEBUG_PRINT(F("Clear buffer..."))
        this->cleanUARTBuffer();

		DEBUG_PRINTLN(F("ok!"))

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


Status LoRa_E220::receiveStruct(void *structureManaged, uint16_t size_) {
	Status result = E220_SUCCESS;

	uint8_t len = this->serialDef.stream->readBytes((uint8_t *) structureManaged, size_);

	DEBUG_PRINT("Available buffer: ");
	DEBUG_PRINT(len);
	DEBUG_PRINT(" structure size: ");
	DEBUG_PRINTLN(size_);

	if (len!=size_){
		if (len==0){
			result = ERR_E220_NO_RESPONSE_FROM_DEVICE;
		}else{
			result = ERR_E220_DATA_SIZE_NOT_MATCH;
		}
	}
	if (result != E220_SUCCESS) return result;

	result = this->waitCompleteResponse(1000);
	if (result != E220_SUCCESS) return result;

	return result;
}

/*

method to set the mode (program, normal, etc.)

*/

Status LoRa_E220::setMode(MODE_TYPE mode) {

	// data sheet claims module needs some extra time after mode setting (2ms)
	// most of my projects uses 10 ms, but 40ms is safer

	this->managedDelay(40);

	if (this->m0Pin == -1 && this->m1Pin == -1) {
		DEBUG_PRINTLN(F("The M0 and M1 pins is not set, this mean that you are connect directly the pins as you need!"))
	}else{
		switch (mode)
		{
		  case MODE_0_NORMAL:
			// Mode 0 | normal operation
			digitalWrite(this->m0Pin, LOW);
			digitalWrite(this->m1Pin, LOW);
			DEBUG_PRINTLN("MODE NORMAL!");
			break;
		  case MODE_1_WOR_TRANSMITTER:
			digitalWrite(this->m0Pin, HIGH);
			digitalWrite(this->m1Pin, LOW);
			DEBUG_PRINTLN("MODE WOR!");
			break;
		  case MODE_2_WOR_RECEIVER:
//		  case MODE_2_PROGRAM:
			digitalWrite(this->m0Pin, LOW);
			digitalWrite(this->m1Pin, HIGH);
			DEBUG_PRINTLN("MODE RECEIVING!");
			break;
		  case MODE_3_CONFIGURATION:
			// Mode 3 | Setting operation
			digitalWrite(this->m0Pin, HIGH);
			digitalWrite(this->m1Pin, HIGH);
			DEBUG_PRINTLN("MODE SLEEP CONFIG!");
			break;

		  default:
			return ERR_E220_INVALID_PARAM;
		}
	}
	// data sheet says 2ms later control is returned, let's give just a bit more time
	// these modules can take time to activate pins
	this->managedDelay(40);

	// wait until aux pin goes back low
	Status res = this->waitCompleteResponse(1000);

	if (res == E220_SUCCESS){
		this->mode = mode;
	}

	return res;
}

MODE_TYPE LoRa_E220::getMode(){
	return this->mode;
}

bool LoRa_E220::writeProgramCommand(PROGRAM_COMMAND cmd, REGISTER_ADDRESS addr, PACKET_LENGHT pl){
	  uint8_t CMD[3] = {cmd, addr, pl};
	  uint8_t size = this->serialDef.stream->write(CMD, 3);

	  DEBUG_PRINTLN(size);

	  this->managedDelay(50);  //need ti check

	  return size!=2;
}

ResponseStructContainer LoRa_E220::getConfiguration(){
	ResponseStructContainer rc;

	rc.status.code = checkUARTConfiguration(MODE_3_PROGRAM);
	if (rc.status.code!=E220_SUCCESS) return rc;

	MODE_TYPE prevMode = this->mode;

	rc.status.code = this->setMode(MODE_3_PROGRAM);
	if (rc.status.code!=E220_SUCCESS) return rc;

	this->writeProgramCommand(READ_CONFIGURATION, REG_ADDRESS_CFG, PL_CONFIGURATION);

	rc.data = malloc(sizeof(Configuration));
	rc.status.code = this->receiveStruct((uint8_t *)rc.data, sizeof(Configuration));

#ifdef LoRa_E220_DEBUG
	 this->printParameters((Configuration *)rc.data);
#endif

	if (rc.status.code!=E220_SUCCESS) {
		this->setMode(prevMode);
		return rc;
	}

	rc.status.code = this->setMode(prevMode);
	if (rc.status.code!=E220_SUCCESS) return rc;

	if (WRONG_FORMAT == ((Configuration *)rc.data)->COMMAND){
		rc.status.code = ERR_E220_WRONG_FORMAT;
	}
	if (RETURNED_COMMAND != ((Configuration *)rc.data)->COMMAND || REG_ADDRESS_CFG!= ((Configuration *)rc.data)->STARTING_ADDRESS || PL_CONFIGURATION!= ((Configuration *)rc.data)->LENGHT){
		rc.status.code = ERR_E220_HEAD_NOT_RECOGNIZED;
	}

	return rc;
}

RESPONSE_STATUS LoRa_E220::checkUARTConfiguration(MODE_TYPE mode){
	if (mode==MODE_3_PROGRAM && this->bpsRate!=UART_BPS_RATE_9600){
		return ERR_E220_WRONG_UART_CONFIG;
	}
	return E220_SUCCESS;
}

ResponseStatus LoRa_E220::setConfiguration(Configuration configuration, PROGRAM_COMMAND saveType){
	ResponseStatus rc;

	rc.code = checkUARTConfiguration(MODE_3_PROGRAM);
	if (rc.code!=E220_SUCCESS) return rc;

	MODE_TYPE prevMode = this->mode;

	rc.code = this->setMode(MODE_3_PROGRAM);
	if (rc.code!=E220_SUCCESS) return rc;

//	this->writeProgramCommand(saveType, REG_ADDRESS_CFG);

//	configuration.HEAD = saveType;
	configuration.COMMAND = saveType;
	configuration.STARTING_ADDRESS = REG_ADDRESS_CFG;
	configuration.LENGHT = PL_CONFIGURATION;

	rc.code = this->sendStruct((uint8_t *)&configuration, sizeof(Configuration));
	if (rc.code!=E220_SUCCESS) {
		this->setMode(prevMode);
		return rc;
	}

	rc.code = this->receiveStruct((uint8_t *)&configuration, sizeof(Configuration));

	#ifdef LoRa_E220_DEBUG
		 this->printParameters((Configuration *)&configuration);
	#endif


	rc.code = this->setMode(prevMode);
	if (rc.code!=E220_SUCCESS) return rc;

	if (WRONG_FORMAT == ((Configuration *)&configuration)->COMMAND){
		rc.code = ERR_E220_WRONG_FORMAT;
	}
	if (RETURNED_COMMAND != ((Configuration *)&configuration)->COMMAND || REG_ADDRESS_CFG!= ((Configuration *)&configuration)->STARTING_ADDRESS || PL_CONFIGURATION!= ((Configuration *)&configuration)->LENGHT){
		rc.code = ERR_E220_HEAD_NOT_RECOGNIZED;
	}

	return rc;
}

ResponseStructContainer LoRa_E220::getModuleInformation(){
	ResponseStructContainer rc;

	rc.status.code = checkUARTConfiguration(MODE_3_PROGRAM);
	if (rc.status.code!=E220_SUCCESS) return rc;

	MODE_TYPE prevMode = this->mode;

	rc.status.code = this->setMode(MODE_3_PROGRAM);
	if (rc.status.code!=E220_SUCCESS) return rc;

	this->writeProgramCommand(READ_CONFIGURATION, REG_ADDRESS_PID, PL_PID);

	rc.data = malloc(sizeof(ModuleInformation));

//	struct ModuleInformation *moduleInformation = (ModuleInformation *)malloc(sizeof(ModuleInformation));
	rc.status.code = this->receiveStruct((uint8_t *)rc.data, sizeof(ModuleInformation));
	if (rc.status.code!=E220_SUCCESS) {
		this->setMode(prevMode);
		return rc;
	}

	rc.status.code = this->setMode(prevMode);
	if (rc.status.code!=E220_SUCCESS) return rc;

//	this->printParameters(*configuration);

	if (WRONG_FORMAT == ((ModuleInformation *)rc.data)->COMMAND){
		rc.status.code = ERR_E220_WRONG_FORMAT;
	}
	if (RETURNED_COMMAND != ((ModuleInformation *)rc.data)->COMMAND || REG_ADDRESS_PID!= ((ModuleInformation *)rc.data)->STARTING_ADDRESS || PL_PID!= ((ModuleInformation *)rc.data)->LENGHT){
		rc.status.code = ERR_E220_HEAD_NOT_RECOGNIZED;
	}

	DEBUG_PRINTLN("----------------------------------------");
	DEBUG_PRINT(F("HEAD: "));  DEBUG_PRINT(((ModuleInformation *)rc.data)->COMMAND, BIN);DEBUG_PRINT(" ");DEBUG_PRINT(((ModuleInformation *)rc.data)->STARTING_ADDRESS, DEC);DEBUG_PRINT(" ");DEBUG_PRINTLN(((ModuleInformation *)rc.data)->LENGHT, HEX);

	DEBUG_PRINT(F("Model no.: "));  DEBUG_PRINTLN(((ModuleInformation *)rc.data)->model, HEX);
	DEBUG_PRINT(F("Version  : "));  DEBUG_PRINTLN(((ModuleInformation *)rc.data)->version, HEX);
	DEBUG_PRINT(F("Features : "));  DEBUG_PRINTLN(((ModuleInformation *)rc.data)->features, HEX);
	DEBUG_PRINT(F("Status : "));  DEBUG_PRINTLN(rc.status.getResponseDescription());
	DEBUG_PRINTLN("----------------------------------------");

//	if (rc.status.code!=E220_SUCCESS) return rc;

//	rc.data = moduleInformation; // malloc(sizeof (moduleInformation));

	return rc;
}


ResponseStatus LoRa_E220::resetModule(){
//	ResponseStatus status;
//
//	status.code = checkUARTConfiguration(MODE_2_PROGRAM);
//	if (status.code!=E220_SUCCESS) return status;
//
//	MODE_TYPE prevMode = this->mode;
//
//	status.code = this->setMode(MODE_2_PROGRAM);
//	if (status.code!=E220_SUCCESS) return status;
//
//	this->writeProgramCommand(WRITE_RESET_MODULE);
//
//	status.code = this->waitCompleteResponse(1000);
//	if (status.code!=E220_SUCCESS)  {
//		this->setMode(prevMode);
//		return status;
//	}
//
//
//	status.code = this->setMode(prevMode);
//	if (status.code!=E220_SUCCESS) return status;
//
//	return status;
	DEBUG_PRINT(F("No information to reset module!"));
	ResponseStatus status;
	status.code = ERR_E220_NOT_IMPLEMENT;
	return status;
}

ResponseContainer LoRa_E220::receiveMessage(){
	return LoRa_E220::receiveMessageComplete(false);
}
ResponseContainer LoRa_E220::receiveMessageRSSI(){
	return LoRa_E220::receiveMessageComplete(true);
}

ResponseContainer LoRa_E220::receiveMessageComplete(bool rssiEnabled){
	ResponseContainer rc;
	rc.status.code = E220_SUCCESS;
	std::string tmpData = this->serialDef.stream->readstd::string();

	DEBUG_PRINTLN(tmpData);

	if (rssiEnabled){
		rc.rssi = tmpData.charAt(tmpData.length()-1);
		rc.data = tmpData.substd::string(0, tmpData.length()-1);
	}else{
		rc.data = tmpData;
	}
	this->cleanUARTBuffer();
	if (rc.status.code!=E220_SUCCESS) {
		return rc;
	}

//	rc.data = message; // malloc(sizeof (moduleInformation));

	return rc;
}

ResponseContainer LoRa_E220::receiveMessageUntil(char delimiter){
	ResponseContainer rc;
	rc.status.code = E220_SUCCESS;
	rc.data = this->serialDef.stream->readstd::stringUntil(delimiter);
//	this->cleanUARTBuffer();
	if (rc.status.code!=E220_SUCCESS) {
		return rc;
	}

//	rc.data = message; // malloc(sizeof (moduleInformation));

	return rc;
}
ResponseContainer LoRa_E220::receiveInitialMessage(uint8_t size){
	ResponseContainer rc;
	rc.status.code = E220_SUCCESS;
	char buff[size];
	uint8_t len = this->serialDef.stream->readBytes(buff, size);
	if (len!=size) {
		if (len==0){
			rc.status.code = ERR_E220_NO_RESPONSE_FROM_DEVICE;
		}else{
			rc.status.code = ERR_E220_DATA_SIZE_NOT_MATCH;
		}
		return rc;
	}

	rc.data = buff; // malloc(sizeof (moduleInformation));

	return rc;
}


ResponseStructContainer LoRa_E220::receiveMessage(const uint8_t size){
	return LoRa_E220::receiveMessageComplete(size, false);
}
ResponseStructContainer LoRa_E220::receiveMessageRSSI(const uint8_t size){
	return LoRa_E220::receiveMessageComplete(size, true);
}

ResponseStructContainer LoRa_E220::receiveMessageComplete(const uint8_t size, bool rssiEnabled){
	ResponseStructContainer rc;

	rc.data = malloc(size);
	rc.status.code = this->receiveStruct((uint8_t *)rc.data, size);
	if (rc.status.code!=E220_SUCCESS) {
		return rc;
	}

	if (rssiEnabled){

		char rssi[1];
		this->serialDef.stream->readBytes(rssi, 1);
		rc.rssi = rssi[0];
	}
	 this->cleanUARTBuffer();

	return rc;
}

ResponseStatus LoRa_E220::sendMessage(const void *message, const uint8_t size){
	ResponseStatus status;
	status.code = this->sendStruct((uint8_t *)message, size);
	if (status.code!=E220_SUCCESS) return status;

	return status;
}
ResponseStatus LoRa_E220::sendMessage(const std::string message){
	DEBUG_PRINT(F("Send message: "));
	DEBUG_PRINT(message);
	byte size = message.length(); // sizeof(message.c_str())+1;
	DEBUG_PRINT(F(" size: "));
	DEBUG_PRINTLN(size);
	char messageFixed[size];
	memcpy(messageFixed,message.c_str(),size);
	DEBUG_PRINTLN(F(" memcpy "));

	ResponseStatus status;
	status.code = this->sendStruct((uint8_t *)&messageFixed, size);
	if (status.code!=E220_SUCCESS) return status;

//	free(messageFixed);
	return status;
}

ResponseStatus LoRa_E220::sendFixedMessage(byte ADDH, byte ADDL, byte CHAN, const std::string message){
//	DEBUG_PRINT("std::string/size: ");
//	DEBUG_PRINT(message);
//	DEBUG_PRINT("/");
	byte size = message.length(); // sizeof(message.c_str())+1;
//	DEBUG_PRINTLN(size);
//
//	#pragma pack(push, 1)
//	struct FixedStransmissionstd::string {
//		byte ADDH = 0;
//		byte ADDL = 0;
//		byte CHAN = 0;
//		char message[];
//	} fixedStransmission;
//	#pragma pack(pop)
//
//	fixedStransmission.ADDH = ADDH;
//	fixedStransmission.ADDL = ADDL;
//	fixedStransmission.CHAN = CHAN;
//	char* msg = (char*)message.c_str();
//	memcpy(fixedStransmission.message, (char*)msg, size);
////	fixedStransmission.message = message;
//
//	DEBUG_PRINT("Message: ");
//	DEBUG_PRINTLN(fixedStransmission.message);
//
//	ResponseStatus status;
//	status.code = this->sendStruct((uint8_t *)&fixedStransmission, sizeof(fixedStransmission));
//	if (status.code!=E220_SUCCESS) return status;
//
//	return status;
	char messageFixed[size];
	memcpy(messageFixed,message.c_str(),size);
	return this->sendFixedMessage(ADDH, ADDL, CHAN, (uint8_t *)messageFixed, size);
}
ResponseStatus LoRa_E220::sendBroadcastFixedMessage(byte CHAN, const std::string message){
	return this->sendFixedMessage(BROADCAST_ADDRESS, BROADCAST_ADDRESS, CHAN, message);
}

typedef struct fixedStransmission
{
	byte ADDH = 0;
	byte ADDL = 0;
	byte CHAN = 0;
	unsigned char message[];
}FixedStransmission;

FixedStransmission *init_stack(int m){
	FixedStransmission *st = (FixedStransmission *)malloc(sizeof(FixedStransmission)+m*sizeof(int));
    return st;
}

ResponseStatus LoRa_E220::sendFixedMessage( byte ADDH,byte ADDL, byte CHAN, const void *message, const uint8_t size){
//	#pragma pack(push, 1)
//	struct FixedStransmission {
//		byte ADDH = 0;
//		byte ADDL = 0;
//		byte CHAN = 0;
//		unsigned char message[];
//	} fixedStransmission;
//	#pragma pack(pop)


	DEBUG_PRINT(ADDH);


	FixedStransmission *fixedStransmission = init_stack(size);

//	STACK *resize_stack(STACK *st, int m){
//	    if (m<=st->max){
//	         return st; /* Take sure do not kill old values */
//	    }
//	    STACK *st = (STACK *)realloc(sizeof(STACK)+m*sizeof(int));
//	    st->max = m;
//	    return st;
//	}

	fixedStransmission->ADDH = ADDH;
	fixedStransmission->ADDL = ADDL;
	fixedStransmission->CHAN = CHAN;
//	fixedStransmission.message = &message;

	memcpy(fixedStransmission->message,(unsigned char*)message,size);

	ResponseStatus status;
	status.code = this->sendStruct((uint8_t *)fixedStransmission, size+3);

	free(fixedStransmission);

	if (status.code!=E220_SUCCESS) return status;

	return status;
}


ConfigurationMessage *init_stack_conf(int m){
	ConfigurationMessage *st = (ConfigurationMessage *)malloc(sizeof(ConfigurationMessage)+m*sizeof(int));
    return st;
}

ResponseStatus LoRa_E220::sendConfigurationMessage( byte ADDH,byte ADDL, byte CHAN, Configuration *configuration, PROGRAM_COMMAND programCommand){
	ResponseStatus rc;

//	rc.code = this->setMode(MODE_2_PROGRAM);
//	if (rc.code!=E220_SUCCESS) return rc;

	configuration->COMMAND = programCommand;
	configuration->STARTING_ADDRESS = REG_ADDRESS_CFG;
	configuration->LENGHT = PL_CONFIGURATION;

	ConfigurationMessage *fixedStransmission = init_stack_conf(sizeof(Configuration));

//	fixedStransmission.message = &message;


	memcpy(fixedStransmission->message,(unsigned char*)configuration,sizeof(Configuration));

	fixedStransmission->specialCommand1 = SPECIAL_WIFI_CONF_COMMAND;
	fixedStransmission->specialCommand2 = SPECIAL_WIFI_CONF_COMMAND;

	DEBUG_PRINTLN(sizeof(Configuration)+2);

	rc = sendFixedMessage(ADDH, ADDL, CHAN, fixedStransmission, sizeof(Configuration)+2);
//
//	ResponseStatus status;
//	status.code = this->sendStruct((uint8_t *)fixedStransmission, sizeof(Configuration)+5);
//	if (status.code!=E220_SUCCESS) return status;

//	free(fixedStransmission);

	return rc;
}

ResponseStatus LoRa_E220::sendBroadcastFixedMessage(byte CHAN, const void *message, const uint8_t size){
	return this->sendFixedMessage(0xFF, 0xFF, CHAN, message, size);
}

#define KeeLoq_NLF		0x3A5C742E

unsigned long LoRa_E220::encrypt(unsigned long data)
{
  unsigned long x = data;
  unsigned long r;
  int keyBitNo, index;
  unsigned long keyBitVal,bitVal;

  for (r = 0; r < 528; r++)
  {
    keyBitNo = r & 63;
    if(keyBitNo < 32)
      keyBitVal = bitRead(this->halfKeyloqKey,keyBitNo); // key low
    else
      keyBitVal = bitRead(this->halfKeyloqKey, keyBitNo - 32);// key hight
    index = 1 * bitRead(x,1) + 2 * bitRead(x,9) + 4 * bitRead(x,20) + 8 * bitRead(x,26) + 16 * bitRead(x,31);
    bitVal = bitRead(x,0) ^ bitRead(x, 16) ^ bitRead(KeeLoq_NLF,index) ^ keyBitVal;
    x = (x>>1) ^ bitVal<<31;
  }
  return x;
}

unsigned long LoRa_E220::decrypt(unsigned long data)
{
  unsigned long x = data;
  unsigned long r;
  int keyBitNo, index;
  unsigned long keyBitVal,bitVal;

  for (r = 0; r < 528; r++)
  {
    keyBitNo = (15-r) & 63;
    if(keyBitNo < 32)
      keyBitVal = bitRead(this->halfKeyloqKey,keyBitNo); // key low
    else
      keyBitVal = bitRead(this->halfKeyloqKey, keyBitNo - 32); // key hight
    index = 1 * bitRead(x,0) + 2 * bitRead(x,8) + 4 * bitRead(x,19) + 8 * bitRead(x,25) + 16 * bitRead(x,30);
    bitVal = bitRead(x,31) ^ bitRead(x, 15) ^ bitRead(KeeLoq_NLF,index) ^ keyBitVal;
    x = (x<<1) ^ bitVal;
  }
  return x;
 }
#ifdef LoRa_E220_DEBUG
void LoRa_E220::printParameters(struct Configuration *configuration) {
	DEBUG_PRINTLN("----------------------------------------");

	DEBUG_PRINT(F("HEAD : "));  DEBUG_PRINT(configuration->COMMAND, HEX);DEBUG_PRINT(" ");DEBUG_PRINT(configuration->STARTING_ADDRESS, HEX);DEBUG_PRINT(" ");DEBUG_PRINTLN(configuration->LENGHT, HEX);
	DEBUG_PRINTLN(F(" "));
	DEBUG_PRINT(F("AddH : "));  DEBUG_PRINTLN(configuration->ADDH, HEX);
	DEBUG_PRINT(F("AddL : "));  DEBUG_PRINTLN(configuration->ADDL, HEX);
	DEBUG_PRINTLN(F(" "));
	DEBUG_PRINT(F("Chan : "));  DEBUG_PRINT(configuration->CHAN, DEC); DEBUG_PRINT(" -> "); DEBUG_PRINTLN(configuration->getChannelDescription());
	DEBUG_PRINTLN(F(" "));
	DEBUG_PRINT(F("SpeedParityBit     : "));  DEBUG_PRINT(configuration->SPED.uartParity, BIN);DEBUG_PRINT(" -> "); DEBUG_PRINTLN(configuration->SPED.getUARTParityDescription());
	DEBUG_PRINT(F("SpeedUARTDatte     : "));  DEBUG_PRINT(configuration->SPED.uartBaudRate, BIN);DEBUG_PRINT(" -> "); DEBUG_PRINTLN(configuration->SPED.getUARTBaudRateDescription());
	DEBUG_PRINT(F("SpeedAirDataRate   : "));  DEBUG_PRINT(configuration->SPED.airDataRate, BIN);DEBUG_PRINT(" -> "); DEBUG_PRINTLN(configuration->SPED.getAirDataRateDescription());
	DEBUG_PRINTLN(F(" "));
	DEBUG_PRINT(F("OptionSubPacketSett: "));  DEBUG_PRINT(configuration->OPTION.subPacketSetting, BIN);DEBUG_PRINT(" -> "); DEBUG_PRINTLN(configuration->OPTION.getSubPacketSetting());
	DEBUG_PRINT(F("OptionTranPower    : "));  DEBUG_PRINT(configuration->OPTION.transmissionPower, BIN);DEBUG_PRINT(" -> "); DEBUG_PRINTLN(configuration->OPTION.getTransmissionPowerDescription());
	DEBUG_PRINT(F("OptionRSSIAmbientNo: "));  DEBUG_PRINT(configuration->OPTION.RSSIAmbientNoise, BIN);DEBUG_PRINT(" -> "); DEBUG_PRINTLN(configuration->OPTION.getRSSIAmbientNoiseEnable());
	DEBUG_PRINTLN(F(" "));
	DEBUG_PRINT(F("TransModeWORPeriod : "));  DEBUG_PRINT(configuration->TRANSMISSION_MODE.WORPeriod, BIN);DEBUG_PRINT(" -> "); DEBUG_PRINTLN(configuration->TRANSMISSION_MODE.getWORPeriodByParamsDescription());
	DEBUG_PRINT(F("TransModeEnableLBT : "));  DEBUG_PRINT(configuration->TRANSMISSION_MODE.enableLBT, BIN);DEBUG_PRINT(" -> "); DEBUG_PRINTLN(configuration->TRANSMISSION_MODE.getLBTEnableByteDescription());
	DEBUG_PRINT(F("TransModeEnableRSSI: "));  DEBUG_PRINT(configuration->TRANSMISSION_MODE.enableRSSI, BIN);DEBUG_PRINT(" -> "); DEBUG_PRINTLN(configuration->TRANSMISSION_MODE.getRSSIEnableByteDescription());
	DEBUG_PRINT(F("TransModeFixedTrans: "));  DEBUG_PRINT(configuration->TRANSMISSION_MODE.fixedTransmission, BIN);DEBUG_PRINT(" -> "); DEBUG_PRINTLN(configuration->TRANSMISSION_MODE.getFixedTransmissionDescription());


	DEBUG_PRINTLN("----------------------------------------");
}
#endif
