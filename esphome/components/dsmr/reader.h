/**
 * Arduino DSMR parser.
 *
 * This software is licensed under the MIT License.
 *
 * Copyright (c) 2015 Matthijs Kooijman <matthijs@stdin.nl>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * P1 reader, that takes care of toggling a request pin, reading data
 * from a serial port and parsing it.
 */

#ifndef DSMR_INCLUDE_READER_H
#define DSMR_INCLUDE_READER_H

#include <Arduino.h>
#include "crc16.h"

#include "parser.h"

namespace dsmr
{

  /**
 * Controls the request pin on the P1 port to enable (periodic)
 * transmission of messages and reads those messages.
 *
 * To enable the request pin, call enable(). This lets the Smart Meter
 * start periodically sending messages. While the request pin is
 * enabled, loop() should be regularly called to read pending bytes.
 *
 * Once a full and correct message is received, loop() (and available())
 * start returning true, until the message is cleared. You can then
 * either read the raw message using raw(), or parse it using parse().
 *
 * The message is cleared when:
 *  - clear() is called
 *  - parse() is called
 *  - loop() is called and the start of a new message is available
 *
 * When disable is called, the request pin is disabled again and any
 * partial message is discarded. Any bytes received while disabled are
 * dropped.
 */
  class P1Reader
  {
  public:
    /**
     * Create a new P1Reader. The stream passed should be the serial
     * port to which the P1 TX pin is connected. The req_pin is the
     * pin connected to the request pin. The pin is configured as an
     * output, the Stream is assumed to be already set up (e.g. baud
     * rate configured).
     */
    P1Reader(Stream *stream, uint8_t req_pin)
        : stream(stream), req_pin(req_pin), once(false), state(State::DISABLED_STATE)
    {
      pinMode(req_pin, OUTPUT);
      digitalWrite(req_pin, LOW);
    }

    /**
     * Enable the request pin, to request data on the P1 port.
     * @param  once    When true, the request pin is automatically
     *                 disabled once a complete and correct message was
     *                 receivedc. When false, the request pin stays
     *                 enabled, so messages will continue to be sent
     *                 periodically.
     */
    void enable(bool once)
    {
      digitalWrite(this->req_pin, HIGH);
      this->state = State::WAITING_STATE;
      this->once = once;
    }

    /* Disable the request pin again, to stop data from being sent on
     * the P1 port. This will also clear any incomplete data that was
     * previously received, but a complete message will be kept until
     * clear() is called.
     */
    void disable()
    {
      digitalWrite(this->req_pin, LOW);
      this->state = State::DISABLED_STATE;
      if (!this->_available)
        this->buffer = "";
      // Clear any pending bytes
      while (this->stream->read() >= 0) /* nothing */
        ;
    }

    /**
     * Returns true when a complete and correct message was received,
     * until it is cleared.
     */
    bool available()
    {
      return this->_available;
    }

    /**
     * Check for new data to read. Should be called regularly, such as
     * once every loop. Returns true if a complete message is available
     * (just like available).
     */
    bool loop()
    {
      while (true)
      {
        if (state == State::CHECKSUM_STATE)
        {
          // Let the Stream buffer the CRC bytes. Convert to size_t to
          // prevent unsigned vs signed comparison
          if ((size_t)this->stream->available() < CrcParser::CRC_LEN)
            return false;

          char buf[CrcParser::CRC_LEN];
          for (uint8_t i = 0; i < CrcParser::CRC_LEN; ++i)
            buf[i] = this->stream->read();

          ParseResult<uint16_t> crc = CrcParser::parse(buf, buf + lengthof(buf));

          // Prepare for next message
          state = State::WAITING_STATE;

          if (!crc.err && crc.result == this->crc)
          {
            // Message complete, checksum correct
            this->_available = true;

            if (once)
              this->disable();

            return true;
          }
        }
        else
        {
          // For other states, read bytes one by one
          int c = this->stream->read();
          if (c < 0)
            return false;

          switch (this->state)
          {
          case State::DISABLED_STATE:
            // Where did this byte come from? Just toss it
            break;
          case State::WAITING_STATE:
            if (c == '/')
            {
              this->state = State::READING_STATE;
              // Include the / in the CRC
              this->crc = _crc16_update(0, c);
              this->clear();
            }
            break;
          case State::READING_STATE:
            // Include the ! in the CRC
            this->crc = _crc16_update(this->crc, c);
            if (c == '!')
              this->state = State::CHECKSUM_STATE;
            else
              buffer.concat((char)c);

            break;
          case State::CHECKSUM_STATE:
            // This cannot happen (given the surrounding if), but the
            // compiler is not smart enough to see this, so list this
            // case to prevent a warning.
            abort();
            break;
          }
        }
      }
      return false;
    }

    /**
     * Returns the data read so far.
     */
    const String &raw()
    {
      return buffer;
    }

    /**
     * If a complete message has been received, parse it and store the
     * result into the ParsedData object passed.
     *
     * After parsing, the message is cleared.
     *
     * If parsing fails, false is returned. If err is passed, the error
     * message is appended to that string.
     */
    template <typename... Ts>
    bool parse(ParsedData<Ts...> *data, String *err)
    {
      const char *str = buffer.c_str(), *end = buffer.c_str() + buffer.length();
      ParseResult<void> res = P1Parser::parse_data(data, str, end);

      if (res.err && err)
        *err = res.fullError(str, end);

      // Clear the message
      this->clear();

      return res.err == NULL;
    }

    /**
     * Clear any complete message from the buffer.
     */
    void clear()
    {
      if (_available)
      {
        buffer = "";
        _available = false;
      }
    }

  protected:
    Stream *stream;
    uint8_t req_pin;
    enum class State : uint8_t
    {
      DISABLED_STATE,
      WAITING_STATE,
      READING_STATE,
      CHECKSUM_STATE,
    };
    bool _available;
    bool once;
    State state;
    String buffer;
    uint16_t crc;
  };

} // namespace dsmr

#endif // DSMR_INCLUDE_READER_H
