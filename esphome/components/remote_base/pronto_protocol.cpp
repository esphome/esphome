 /*
  * @file irPronto.cpp
  * @brief In this file, the functions IRrecv::compensateAndPrintPronto and IRsend::sendPronto are defined.
  *
  * See http://www.harctoolbox.org/Glossary.html#ProntoSemantics
  * Pronto database http://www.remotecentral.com/search.htm
  *
  *  This file is part of Arduino-IRremote https://github.com/Arduino-IRremote/Arduino-IRremote.
  *
  ************************************************************************************
  * MIT License
  *
  * Copyright (c) 2020 Bengt Martensson
  *
  * Permission is hereby granted, free of charge, to any person obtaining a copy
  * of this software and associated documentation files (the "Software"), to deal
  * in the Software without restriction, including without limitation the rights
  * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  * copies of the Software, and to permit persons to whom the Software is furnished
  * to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in all
  * copies or substantial portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
  * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
  * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
  * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
  * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
  * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  *
  ************************************************************************************
  */
  
#include "pronto_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.pronto";

// DO NOT EXPORT from this file
static const uint16_t MICROSECONDS_T_MAX = 0xFFFFU;
static const uint16_t learnedToken = 0x0000U;
static const uint16_t learnedNonModulatedToken = 0x0100U;
static const unsigned int bitsInHexadecimal = 4U;
static const unsigned int digitsInProntoNumber = 4U;
static const unsigned int numbersInPreamble = 4U;
static const unsigned int hexMask = 0xFU;
static const uint32_t referenceFrequency = 4145146UL;
static const uint16_t fallbackFrequency = 64767U; // To use with frequency = 0;
static const uint32_t microsecondsInSeconds = 1000000UL;
static const uint16_t PRONTO_DEFAULT_GAP = 45000;

static unsigned int toFrequencyKHz(uint16_t code) {
  return ((referenceFrequency / code) + 500) / 1000;
}

/*
 * Parse the string given as Pronto Hex, and send it a number of times given as argument.
 */
void ProntoProtocol::sendPronto(RemoteTransmitData *dst, const uint16_t *data, unsigned int length) {
  unsigned int timebase = (microsecondsInSeconds * data[1] + referenceFrequency / 2) / referenceFrequency;
  unsigned int khz;
  switch (data[0]) {
    case learnedToken: // normal, "learned"
      khz = toFrequencyKHz(data[1]);
      break;
    case learnedNonModulatedToken: // non-demodulated, "learned"
      khz = 0U;
      break;
    default:
      return; // There are other types, but they are not handled yet.
  }
  ESP_LOGD(TAG, "Send Pronto: frequency=%dkHz", khz);
  dst->set_carrier_frequency(khz * 1000);

  unsigned int intros = 2 * data[2];
  unsigned int repeats = 2 * data[3];
  ESP_LOGD(TAG, "Send Pronto: intros=%d", intros);
  ESP_LOGD(TAG, "Send Pronto: repeats=%d", repeats);
  if (numbersInPreamble + intros + repeats != length) { // inconsistent sizes
    return;
  }

  /*
   * Generate a new microseconds timing array for sendRaw.
   * If recorded by IRremote, intro contains the whole IR data and repeat is empty
   */
  dst->reserve(intros + repeats);

  for (unsigned int i = 0; i < intros + repeats; i+=2) {
    uint32_t duration0 = ((uint32_t) data[i + 0 + numbersInPreamble]) * timebase;
    duration0 = duration0 < MICROSECONDS_T_MAX ? duration0 : MICROSECONDS_T_MAX;

    uint32_t duration1 = ((uint32_t) data[i + 1 + numbersInPreamble]) * timebase;
    duration1 = duration1 < MICROSECONDS_T_MAX ? duration1 : MICROSECONDS_T_MAX;

    dst->item(duration0, duration1);
  }
}

void ProntoProtocol::sendPronto(RemoteTransmitData *dst, const std::string str) {
  size_t len = str.length() / (digitsInProntoNumber + 1) + 1;
  uint16_t data[len];
  const char *p = str.c_str();
  char *endptr[1];
  for (unsigned int i = 0; i < len; i++) {
    long x = strtol(p, endptr, 16);
    if (x == 0 && i >= numbersInPreamble) {
      // Alignment error?, bail immediately (often right result).
      len = i;
      break;
    }
    data[i] = static_cast<uint16_t>(x); // If input is conforming, there can be no overflow!
    p = *endptr;
  }
  sendPronto(dst, data, len);
}

void ProntoProtocol::encode(RemoteTransmitData *dst, const ProntoData &data) {
   sendPronto(dst, data.data);
}

optional<ProntoData> ProntoProtocol::decode(RemoteReceiveData src) {
  return {};
}

void ProntoProtocol::dump(const ProntoData &data) {
  ESP_LOGD(TAG, "Received Pronto: data=%s", data.data.c_str());
}

}  // namespace remote_base
}  // namespace esphome
