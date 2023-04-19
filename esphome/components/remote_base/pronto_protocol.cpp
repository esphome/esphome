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

bool ProntoData::operator==(const ProntoData &rhs) const {
  std::vector<uint16_t> data1 = encode_pronto(data);
  std::vector<uint16_t> data2 = encode_pronto(rhs.data);

  uint32_t total_diff = 0;
  // Don't need to check the last one, it's the large gap at the end.
  for (std::vector<uint16_t>::size_type i = 0; i < data1.size() - 1; ++i) {
    int diff = data2[i] - data1[i];
    diff *= diff;
    if (diff > 9)
      return false;

    total_diff += diff;
  }

  return total_diff <= data1.size() * 3;
}

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
static const uint16_t MARK_EXCESS_MICROS = 20;

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
    ESP_LOGE(TAG, "Inconsistent data, not sending");
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

std::vector<uint16_t> encode_pronto(const std::string &str) {
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

  return data;
}

void ProntoProtocol::send_pronto_(RemoteTransmitData *dst, const std::string &str) {
  std::vector<uint16_t> data = encode_pronto(str);
  send_pronto_(dst, data);
}

void ProntoProtocol::encode(RemoteTransmitData *dst, const ProntoData &data) { send_pronto_(dst, data.data); }

uint16_t ProntoProtocol::effective_frequency_(uint16_t frequency) {
  return frequency > 0 ? frequency : FALLBACK_FREQUENCY;
}

uint16_t ProntoProtocol::to_timebase_(uint16_t frequency) {
  return MICROSECONDS_IN_SECONDS / effective_frequency_(frequency);
}

uint16_t ProntoProtocol::to_frequency_code_(uint16_t frequency) {
  return REFERENCE_FREQUENCY / effective_frequency_(frequency);
}

std::string ProntoProtocol::dump_digit_(uint8_t x) {
  return std::string(1, (char) (x <= 9 ? ('0' + x) : ('A' + (x - 10))));
}

std::string ProntoProtocol::dump_number_(uint16_t number, bool end /* = false */) {
  std::string num;

  for (uint8_t i = 0; i < DIGITS_IN_PRONTO_NUMBER; ++i) {
    uint8_t shifts = BITS_IN_HEXADECIMAL * (DIGITS_IN_PRONTO_NUMBER - 1 - i);
    num += dump_digit_((number >> shifts) & HEX_MASK);
  }

  if (!end)
    num += ' ';

  return num;
}

std::string ProntoProtocol::dump_duration_(uint32_t duration, uint16_t timebase, bool end /* = false */) {
  return dump_number_((duration + timebase / 2) / timebase, end);
}

std::string ProntoProtocol::compensate_and_dump_sequence_(std::vector<int32_t> *data, uint16_t timebase) {
  std::string out;

  for (std::vector<int32_t>::size_type i = 0; i < data->size() - 1; i++) {
    int32_t t_length = data->at(i);
    uint32_t t_duration;
    if (t_length > 0) {
      // Mark
      t_duration = t_length - MARK_EXCESS_MICROS;
    } else {
      t_duration = -t_length + MARK_EXCESS_MICROS;
    }
    out += dump_duration_(t_duration, timebase);
  }

  // append minimum gap
  out += dump_duration_(PRONTO_DEFAULT_GAP, timebase, true);

  return out;
}

optional<ProntoData> ProntoProtocol::decode(RemoteReceiveData src) {
  ProntoData out;

  uint16_t frequency = 38000U;
  std::vector<int32_t> *data = src.get_raw_data();
  std::string prontodata;

  prontodata += dump_number_(frequency > 0 ? LEARNED_TOKEN : LEARNED_NON_MODULATED_TOKEN);
  prontodata += dump_number_(to_frequency_code_(frequency));
  prontodata += dump_number_((data->size() + 1) / 2);
  prontodata += dump_number_(0);
  uint16_t timebase = to_timebase_(frequency);
  prontodata += compensate_and_dump_sequence_(data, timebase);

  out.data = prontodata;

  return out;
}

void ProntoProtocol::dump(const ProntoData &data) {
  std::string first, rest;
  if (data.data.size() < 230) {
    first = data.data;
  } else {
    first = data.data.substr(0, 229);
    rest = data.data.substr(230);
  }
  ESP_LOGD(TAG, "Received Pronto: data=%s", first.c_str());
  if (!rest.empty()) {
    ESP_LOGD(TAG, "%s", rest.c_str());
  }
}

}  // namespace remote_base
}  // namespace esphome
