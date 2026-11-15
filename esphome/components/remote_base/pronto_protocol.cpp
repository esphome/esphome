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
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>

namespace esphome::remote_base {

ESPHOME_LOG_TAG(TAG, "remote.pronto");

bool ProntoData::operator==(const ProntoData &rhs) const {
  std::vector<uint16_t> data1 = encode_pronto(data);
  std::vector<uint16_t> data2 = encode_pronto(rhs.data);

  if (data1.size() != data2.size() || data1.empty()) {
    return false;
  }

  uint32_t total_diff = 0;
  // Don't need to check the last one, it's the large gap at the end.
  for (size_t i = 0; i < data1.size() - 1; ++i) {
    int diff = data2[i] - data1[i];
    diff *= diff;
    if (rhs.delta == -1 && diff > 9)
      return false;

    total_diff += diff;
  }

  return total_diff <= (rhs.delta == -1 ? data1.size() * 3 : rhs.delta);
}

// DO NOT EXPORT from this file
static constexpr uint16_t MICROSECONDS_T_MAX = 0xFFFFU;
static constexpr uint16_t LEARNED_TOKEN = 0x0000U;
static constexpr uint16_t LEARNED_NON_MODULATED_TOKEN = 0x0100U;
static constexpr uint16_t BITS_IN_HEXADECIMAL = 4U;
static constexpr uint16_t DIGITS_IN_PRONTO_NUMBER = 4U;
static constexpr uint16_t NUMBERS_IN_PREAMBLE = 4U;
static constexpr uint16_t HEX_MASK = 0xFU;
static constexpr uint32_t REFERENCE_FREQUENCY = 4145146UL;
static constexpr uint16_t FALLBACK_FREQUENCY = 64767U;  // To use with frequency = 0;
static constexpr uint32_t MICROSECONDS_IN_SECONDS = 1000000UL;
static constexpr uint16_t PRONTO_DEFAULT_GAP = 45000;
static constexpr uint16_t MARK_EXCESS_MICROS = 20;
static constexpr size_t PRONTO_LOG_CHUNK_SIZE = 230;

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
  ESP_LOGD(TAG, "Send Pronto: intros=%d, repeats=%d", intros, repeats);
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

// decode() reports every frame as learned at 38 kHz
static constexpr uint32_t DECODE_FREQUENCY = 38000U;
static constexpr uint16_t DECODE_TIMEBASE = MICROSECONDS_IN_SECONDS / DECODE_FREQUENCY;
// Four hex digits and a space
static constexpr size_t TEXT_NUMBER_LENGTH = DIGITS_IN_PRONTO_NUMBER + 1;

// The index-th number of the text: the preamble, then each timing in timebase units
static uint16_t text_number(RawTimingsView data, size_t index) {
  switch (index) {
    case 0:
      return LEARNED_TOKEN;
    case 1:
      return REFERENCE_FREQUENCY / DECODE_FREQUENCY;
    case 2:
      return (data.size() + 1) / 2;
    case 3:
      return 0;
    default:
      break;
  }
  const int32_t t_length = data[index - NUMBERS_IN_PREAMBLE];
  const uint32_t t_duration = t_length > 0 ? t_length - MARK_EXCESS_MICROS : -t_length + MARK_EXCESS_MICROS;
  return (t_duration + DECODE_TIMEBASE / 2) / DECODE_TIMEBASE;
}

size_t ProntoProtocol::text_length(RawTimingsView data) {
  return (NUMBERS_IN_PREAMBLE + data.size()) * TEXT_NUMBER_LENGTH;
}

void ProntoProtocol::write_text(RawTimingsView data, size_t offset, char *out, size_t len) {
  size_t index = offset / TEXT_NUMBER_LENGTH;
  size_t pos = offset % TEXT_NUMBER_LENGTH;
  while (len > 0) {
    const uint16_t number = text_number(data, index++);
    for (; pos < TEXT_NUMBER_LENGTH && len > 0; pos++, len--) {
      const uint8_t shift = BITS_IN_HEXADECIMAL * (DIGITS_IN_PRONTO_NUMBER - 1 - pos);
      *out++ = pos < DIGITS_IN_PRONTO_NUMBER ? format_hex_pretty_char((number >> shift) & HEX_MASK) : ' ';
    }
    pos = 0;
  }
}

optional<ProntoData> ProntoProtocol::decode(RemoteReceiveData src) {
  const auto data = src.get_raw_data();
  ProntoData out;
  out.data.resize(text_length(data));
  write_text(data, 0, out.data.data(), out.data.size());
  out.delta = -1;
  return out;
}

void ProntoProtocol::dump(const ProntoData &data) {
  ESP_LOGI(TAG, "Received Pronto: data=");

  const char *ptr = data.data.c_str();
  size_t remaining = data.data.size();

  // Log in chunks, always logging at least once (even for empty string)
  do {
    size_t chunk_size = remaining < PRONTO_LOG_CHUNK_SIZE ? remaining : PRONTO_LOG_CHUNK_SIZE;
    ESP_LOGI(TAG, "%.*s", (int) chunk_size, ptr);
    ptr += chunk_size;
    remaining -= chunk_size;
  } while (remaining > 0);
}

bool ProntoDumper::dump(RemoteReceiveData src) {
  const auto data = src.get_raw_data();
  ESP_LOGI(TAG, "Received Pronto: data=");
  char chunk[PRONTO_LOG_CHUNK_SIZE];
  const size_t total = ProntoProtocol::text_length(data);
  for (size_t offset = 0; offset < total; offset += PRONTO_LOG_CHUNK_SIZE) {
    const size_t len = std::min(total - offset, PRONTO_LOG_CHUNK_SIZE);
    ProntoProtocol::write_text(data, offset, chunk, len);
    ESP_LOGI(TAG, "%.*s", (int) len, chunk);
  }
  return true;
}

}  // namespace esphome::remote_base
