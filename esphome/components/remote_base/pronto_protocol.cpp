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
static const uint16_t LEARNED_TOKEN = 0x0000U;
static const uint16_t LEARNED_NON_MODULATED_TOKEN = 0x0100U;
static const uint16_t BITS_IN_HEXADECIMAL = 4U;
static const uint16_t DIGITS_IN_PRONTO_NUMBER = 4U;
static const uint16_t NUMBERS_IN_PREAMBLE = 4U;
static const uint16_t HEX_MASK = 0xFU;
static const uint32_t REFERENCE_FREQUENCY = 4145146UL;
static const uint16_t FALLBACK_FREQUENCY = 64767U;  // To use with frequency = 0;
static const uint32_t MICROSECONDS_IN_SECONDS = 1000000UL;
static const uint16_t PRONTO_DEFAULT_GAP = 45000;

static uint16_t to_frequency_k_hz(uint16_t code) {
  if (code == 0)
    return 0;

  return ((REFERENCE_FREQUENCY / code) + 500) / 1000;
}

/*
 * Parse the string given as Pronto Hex, and send it a number of times given as argument.
 */
void ProntoProtocol::send_pronto_(RemoteTransmitData *dst, const std::vector<uint16_t> &data) {
  if (data.size() < 4)
    return;

  uint16_t timebase = (MICROSECONDS_IN_SECONDS * data[1] + REFERENCE_FREQUENCY / 2) / REFERENCE_FREQUENCY;
  uint16_t khz;
  switch (data[0]) {
    case LEARNED_TOKEN:  // normal, "learned"
      khz = to_frequency_k_hz(data[1]);
      break;
    case LEARNED_NON_MODULATED_TOKEN:  // non-demodulated, "learned"
      khz = 0U;
      break;
    default:
      return;  // There are other types, but they are not handled yet.
  }
  ESP_LOGD(TAG, "Send Pronto: frequency=%dkHz", khz);
  dst->set_carrier_frequency(khz * 1000);

  uint16_t intros = 2 * data[2];
  uint16_t repeats = 2 * data[3];
  ESP_LOGD(TAG, "Send Pronto: intros=%d", intros);
  ESP_LOGD(TAG, "Send Pronto: repeats=%d", repeats);
  if (NUMBERS_IN_PREAMBLE + intros + repeats != data.size()) {  // inconsistent sizes
    return;
  }

  /*
   * Generate a new microseconds timing array for sendRaw.
   * If recorded by IRremote, intro contains the whole IR data and repeat is empty
   */
  dst->reserve(intros + repeats);

  for (uint16_t i = 0; i < intros + repeats; i += 2) {
    uint32_t duration0 = ((uint32_t) data[i + 0 + NUMBERS_IN_PREAMBLE]) * timebase;
    duration0 = duration0 < MICROSECONDS_T_MAX ? duration0 : MICROSECONDS_T_MAX;

    uint32_t duration1 = ((uint32_t) data[i + 1 + NUMBERS_IN_PREAMBLE]) * timebase;
    duration1 = duration1 < MICROSECONDS_T_MAX ? duration1 : MICROSECONDS_T_MAX;

    dst->item(duration0, duration1);
  }
}

void ProntoProtocol::send_pronto_(RemoteTransmitData *dst, const std::string &str) {
  size_t len = str.length() / (DIGITS_IN_PRONTO_NUMBER + 1) + 1;
  std::vector<uint16_t> data;
  const char *p = str.c_str();
  char *endptr[1];

  for (size_t i = 0; i < len; i++) {
    uint16_t x = strtol(p, endptr, 16);
    if (x == 0 && i >= NUMBERS_IN_PREAMBLE) {
      // Alignment error?, bail immediately (often right result).
      break;
    }
    data.push_back(x);  // If input is conforming, there can be no overflow!
    p = *endptr;
  }
  send_pronto_(dst, data);
}

void ProntoProtocol::encode(RemoteTransmitData *dst, const ProntoData &data) { send_pronto_(dst, data.data); }

optional<ProntoData> ProntoProtocol::decode(RemoteReceiveData src) { return {}; }

void ProntoProtocol::dump(const ProntoData &data) { ESP_LOGD(TAG, "Received Pronto: data=%s", data.data.c_str()); }

}  // namespace remote_base
}  // namespace esphome
