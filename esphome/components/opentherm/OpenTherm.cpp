/*
OpenTherm.cpp - OpenTherm Communication Library For Arduino, ESP8266
Copyright 2018, Ihor Melnyk
*/

#include "OpenTherm.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace opentherm {

OpenTherm::OpenTherm(InternalGPIOPin *in_pin, InternalGPIOPin *out_pin, bool is_slave)
    : status(OpenThermStatus::NOT_INITIALIZED),
      in_pin_(in_pin),
      out_pin_(out_pin),
      is_slave_(is_slave),
      response(0),
      responseStatus(OpenThermResponseStatus::NONE),
      responseTimestamp(0),
      processResponseCallback(nullptr) {
  isr_in_pin_ = in_pin->to_isr();
  isr_out_pin_ = out_pin->to_isr();
}

void OpenTherm::begin() { begin(nullptr); }

void OpenTherm::begin(void (*process_response_callback)(uint32_t, OpenThermResponseStatus)) {
  in_pin_->pin_mode(gpio::FLAG_INPUT);
  out_pin_->pin_mode(gpio::FLAG_OUTPUT);

  in_pin_->attach_interrupt(handleInterrupt, this, gpio::INTERRUPT_ANY_EDGE);

  activateBoiler();
  status = OpenThermStatus::READY;
  this->processResponseCallback = process_response_callback;
}

bool IRAM_ATTR OpenTherm::isReady() { return status == OpenThermStatus::READY; }

int IRAM_ATTR OpenTherm::readState() { return isr_in_pin_.digital_read(); }

void OpenTherm::setActiveState() { isr_out_pin_.digital_write(false); }

void OpenTherm::setIdleState() { isr_out_pin_.digital_write(true); }

void OpenTherm::activateBoiler() {
  setIdleState();
  delay(1000);
}

void OpenTherm::sendBit(bool high) {
  if (high) {
    setActiveState();
  } else {
    setIdleState();
  }

  delayMicroseconds(500);

  if (high) {
    setIdleState();
  } else {
    setActiveState();
  }

  delayMicroseconds(500);
}

bool OpenTherm::sendRequestAync(unsigned long request) {
  // Serial.println("Request: " + String(request, HEX));
  bool ready;

  {
    InterruptLock const lock;
    ready = isReady();
  }

  if (!ready)
    return false;

  status = OpenThermStatus::REQUEST_SENDING;
  response = 0;
  responseStatus = OpenThermResponseStatus::NONE;

  sendBit(true);  // start bit
  for (int i = 31; i >= 0; i--) {
    sendBit(bitRead(request, i));
  }
  sendBit(true);  // stop bit
  setIdleState();

  status = OpenThermStatus::RESPONSE_WAITING;
  responseTimestamp = micros();
  return true;
}

unsigned long OpenTherm::sendRequest(unsigned long request) {
  if (!sendRequestAync(request))
    return 0;
  while (!isReady()) {
    process();
    yield();
  }
  return response;
}

bool OpenTherm::sendResponse(unsigned long request) {
  status = OpenThermStatus::REQUEST_SENDING;
  response = 0;
  responseStatus = OpenThermResponseStatus::NONE;

  sendBit(true);  // start bit
  for (int i = 31; i >= 0; i--) {
    sendBit(bitRead(request, i));
  }
  sendBit(true);  // stop bit
  setIdleState();
  status = OpenThermStatus::READY;
  return true;
}

unsigned long OpenTherm::getLastResponse() { return response; }

OpenThermResponseStatus OpenTherm::getLastResponseStatus() { return responseStatus; }

void IRAM_ATTR OpenTherm::handleInterrupt(OpenTherm *arg) {
  if (arg->isReady()) {
    if (arg->is_slave_ && arg->readState()) {
      arg->status = OpenThermStatus::RESPONSE_WAITING;
    } else {
      return;
    }
  }

  uint32_t const newTs = micros();
  if (arg->status == OpenThermStatus::RESPONSE_WAITING) {
    if (arg->readState()) {
      arg->status = OpenThermStatus::RESPONSE_START_BIT;
      arg->responseTimestamp = newTs;
    } else {
      arg->status = OpenThermStatus::RESPONSE_INVALID;
      arg->responseTimestamp = newTs;
    }
  } else if (arg->status == OpenThermStatus::RESPONSE_START_BIT) {
    if ((newTs - arg->responseTimestamp < 750) && !arg->readState()) {
      arg->status = OpenThermStatus::RESPONSE_RECEIVING;
      arg->responseTimestamp = newTs;
      arg->responseBitIndex = 0;
    } else {
      arg->status = OpenThermStatus::RESPONSE_INVALID;
      arg->responseTimestamp = newTs;
    }
  } else if (arg->status == OpenThermStatus::RESPONSE_RECEIVING) {
    if ((newTs - arg->responseTimestamp) > 750) {
      if (arg->responseBitIndex < 32) {
        arg->response = (arg->response << 1) | !arg->readState();
        arg->responseTimestamp = newTs;
        arg->responseBitIndex++;
      } else {  // stop bit
        arg->status = OpenThermStatus::RESPONSE_READY;
        arg->responseTimestamp = newTs;
      }
    }
  }
}

void OpenTherm::process() {
  OpenThermStatus st;
  unsigned long ts;

  {
    InterruptLock const lock;

    st = status;
    ts = responseTimestamp;
  }

  if (st == OpenThermStatus::READY)
    return;
  unsigned long newTs = micros();
  if (st != OpenThermStatus::NOT_INITIALIZED && st != OpenThermStatus::DELAY && (newTs - ts) > 1000000) {
    status = OpenThermStatus::READY;
    responseStatus = OpenThermResponseStatus::TIMEOUT;
    if (processResponseCallback != nullptr) {
      processResponseCallback(response, responseStatus);
    }
  } else if (st == OpenThermStatus::RESPONSE_INVALID) {
    status = OpenThermStatus::DELAY;
    responseStatus = OpenThermResponseStatus::INVALID;
    if (processResponseCallback != nullptr) {
      processResponseCallback(response, responseStatus);
    }
  } else if (st == OpenThermStatus::RESPONSE_READY) {
    status = OpenThermStatus::DELAY;
    responseStatus = (is_slave_ ? isValidRequest(response) : isValidResponse(response))
                         ? OpenThermResponseStatus::SUCCESS
                         : OpenThermResponseStatus::INVALID;
    if (processResponseCallback != nullptr) {
      processResponseCallback(response, responseStatus);
    }
  } else if (st == OpenThermStatus::DELAY) {
    if ((newTs - ts) > 100000) {
      status = OpenThermStatus::READY;
    }
  }
}

bool OpenTherm::parity(unsigned long frame)  // odd parity
{
  uint8_t p = 0;
  while (frame > 0) {
    if (frame & 1)
      p++;
    frame = frame >> 1;
  }
  return (p & 1);
}

OpenThermMessageType OpenTherm::getMessageType(unsigned long message) {
  OpenThermMessageType msg_type = static_cast<OpenThermMessageType>((message >> 28) & 7);
  return msg_type;
}

OpenThermMessageID OpenTherm::getDataID(unsigned long frame) { return (OpenThermMessageID) ((frame >> 16) & 0xFF); }

unsigned long OpenTherm::buildRequest(OpenThermMessageType type, OpenThermMessageID id, unsigned int data) {
  unsigned long request = data;
  if (type == OpenThermMessageType::WRITE_DATA) {
    request |= 1ul << 28;
  }
  request |= ((unsigned long) id) << 16;
  if (parity(request))
    request |= (1ul << 31);
  return request;
}

unsigned long OpenTherm::buildResponse(OpenThermMessageType type, OpenThermMessageID id, unsigned int data) {
  unsigned long response = data;
  response |= ((unsigned long) type) << 28;
  response |= ((unsigned long) id) << 16;
  if (parity(response))
    response |= (1ul << 31);
  return response;
}

bool OpenTherm::isValidResponse(unsigned long response) {
  if (parity(response))
    return false;
  uint8_t msgType = (response << 1) >> 29;
  return msgType == READ_ACK || msgType == WRITE_ACK;
}

bool OpenTherm::isValidRequest(unsigned long request) {
  if (parity(request))
    return false;
  uint8_t msgType = (request << 1) >> 29;
  return msgType == READ_DATA || msgType == WRITE_DATA;
}

void OpenTherm::end() { in_pin_->detach_interrupt(); }

const char *OpenTherm::statusToString(OpenThermResponseStatus status) {
  switch (status) {
    case NONE:
      return "NONE";
    case SUCCESS:
      return "SUCCESS";
    case INVALID:
      return "INVALID";
    case TIMEOUT:
      return "TIMEOUT";
    default:
      return "UNKNOWN";
  }
}

const char *OpenTherm::messageTypeToString(OpenThermMessageType message_type) {
  switch (message_type) {
    case READ_DATA:
      return "READ_DATA";
    case WRITE_DATA:
      return "WRITE_DATA";
    case INVALID_DATA:
      return "INVALID_DATA";
    case RESERVED:
      return "RESERVED";
    case READ_ACK:
      return "READ_ACK";
    case WRITE_ACK:
      return "WRITE_ACK";
    case DATA_INVALID:
      return "DATA_INVALID";
    case UNKNOWN_DATA_ID:
      return "UNKNOWN_DATA_ID";
    default:
      return "UNKNOWN";
  }
}

// building requests

unsigned long OpenTherm::buildSetBoilerStatusRequest(bool enableCentralHeating, bool enableHotWater, bool enableCooling,
                                                     bool enableOutsideTemperatureCompensation,
                                                     bool enableCentralHeating2) {
  unsigned int data = enableCentralHeating | (enableHotWater << 1) | (enableCooling << 2) |
                      (enableOutsideTemperatureCompensation << 3) | (enableCentralHeating2 << 4);
  data <<= 8;
  return buildRequest(OpenThermMessageType::READ_DATA, OpenThermMessageID::Status, data);
}

unsigned long OpenTherm::buildSetBoilerTemperatureRequest(float temperature) {
  unsigned int data = temperatureToData(temperature);
  return buildRequest(OpenThermMessageType::WRITE_DATA, OpenThermMessageID::TSet, data);
}

unsigned long OpenTherm::buildGetBoilerTemperatureRequest() {
  return buildRequest(OpenThermMessageType::READ_DATA, OpenThermMessageID::Tboiler, 0);
}

// parsing responses
bool OpenTherm::isFault(unsigned long response) { return response & 0x1; }

bool OpenTherm::isCentralHeatingActive(unsigned long response) { return response & 0x2; }

bool OpenTherm::isHotWaterActive(unsigned long response) { return response & 0x4; }

bool OpenTherm::isFlameOn(unsigned long response) { return response & 0x8; }

bool OpenTherm::isCoolingActive(unsigned long response) { return response & 0x10; }

bool OpenTherm::isDiagnostic(unsigned long response) { return response & 0x40; }

uint16_t OpenTherm::getUInt(const unsigned long response) const {
  const uint16_t u88 = response & 0xffff;
  return u88;
}

float OpenTherm::getFloat(const unsigned long response) const {
  const uint16_t u88 = getUInt(response);
  const float f = (u88 & 0x8000) ? -(0x10000L - u88) / 256.0f : u88 / 256.0f;
  return f;
}

unsigned int OpenTherm::temperatureToData(float temperature) {
  if (temperature < 0)
    temperature = 0;
  if (temperature > 100)
    temperature = 100;
  unsigned int data = (unsigned int) (temperature * 256);
  return data;
}

// basic requests

unsigned long OpenTherm::setBoilerStatus(bool enableCentralHeating, bool enableHotWater, bool enableCooling,
                                         bool enableOutsideTemperatureCompensation, bool enableCentralHeating2) {
  return sendRequest(buildSetBoilerStatusRequest(enableCentralHeating, enableHotWater, enableCooling,
                                                 enableOutsideTemperatureCompensation, enableCentralHeating2));
}

bool OpenTherm::setBoilerTemperature(float temperature) {
  unsigned long response = sendRequest(buildSetBoilerTemperatureRequest(temperature));
  return isValidResponse(response);
}

float OpenTherm::getBoilerTemperature() {
  unsigned long response = sendRequest(buildGetBoilerTemperatureRequest());
  return isValidResponse(response) ? getFloat(response) : 0;
}

float OpenTherm::getReturnTemperature() {
  unsigned long response = sendRequest(buildRequest(OpenThermMessageType::READ, OpenThermMessageID::Tret, 0));
  return isValidResponse(response) ? getFloat(response) : 0;
}

bool OpenTherm::setDHWSetpoint(float temperature) {
  unsigned int data = temperatureToData(temperature);
  unsigned long response =
      sendRequest(buildRequest(OpenThermMessageType::WRITE_DATA, OpenThermMessageID::TdhwSet, data));
  return isValidResponse(response);
}

float OpenTherm::getDHWTemperature() {
  unsigned long response = sendRequest(buildRequest(OpenThermMessageType::READ_DATA, OpenThermMessageID::Tdhw, 0));
  return isValidResponse(response) ? getFloat(response) : 0;
}

float OpenTherm::getModulation() {
  unsigned long response = sendRequest(buildRequest(OpenThermMessageType::READ, OpenThermMessageID::RelModLevel, 0));
  return isValidResponse(response) ? getFloat(response) : 0;
}

float OpenTherm::getPressure() {
  unsigned long response = sendRequest(buildRequest(OpenThermMessageType::READ, OpenThermMessageID::CHPressure, 0));
  return isValidResponse(response) ? getFloat(response) : 0;
}

unsigned char OpenTherm::getFault() {
  return ((sendRequest(buildRequest(OpenThermMessageType::READ, OpenThermMessageID::ASFflags, 0)) >> 8) & 0xff);
}

}  // namespace opentherm
}  // namespace esphome