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
 * Various utility functions
 */

#pragma once

#include <Arduino.h>

namespace dsmr
{

  /**
 * Small utility to get the length of an array at compiletime.
 */
  template <typename T, unsigned int sz>
  inline unsigned int lengthof(const T (&)[sz]) { return sz; }

  // Hack until https://github.com/arduino/Arduino/pull/1936 is merged.
  // This appends the given number of bytes from the given C string to the
  // given Arduino string, without requiring a trailing NUL.
  // Requires that there _is_ room for nul-termination
  static void concat_hack(String &s, const char *append, size_t n)
  {
    // Add null termination. Inefficient, but it works...
    char buf[n + 1];
    memcpy(buf, append, n);
    buf[n] = 0;
    s.concat(buf);
  }

  /**
 * The ParseResult<T> class wraps the result of a parse function. The type
 * of the result is passed as a template parameter and can be void to
 * not return any result.
 *
 * A ParseResult can either:
 *  - Return an error. In this case, err is set to an error message, ctx
 *    is optionally set to where the error occurred. The result (if any)
 *    and the next pointer are meaningless.
 *  - Return succesfully. In this case, err and ctx are NULL, result
 *    contains the result (if any) and next points one past the last
 *    byte processed by the parser.
 *
 * The ParseResult class has some convenience functions:
 *  - succeed(result): sets the result to the given value and returns
 *    the ParseResult again.
 *  - fail(err): Set the err member to the error message passed,
 *    optionally sets the ctx and return the ParseResult again.
 *  - until(next): Set the next member and return the ParseResult again.
 *
 * Furthermore, ParseResults can be implicitely converted to other
 * types. In this case, the error message, context and and next pointer are
 * conserved, the return value is reset to the default value for the
 * target type.
 *
 * Note that ctx points into the string being parsed, so it does not
 * need to be freed, lives as long as the original string and is
 * probably way longer that needed.
 */

  // Superclass for ParseResult so we can specialize for void without
  // having to duplicate all content
  template <typename P, typename T>
  struct _ParseResult
  {
    T result;

    P &succeed(T &result)
    {
      this->result = result;
      return *static_cast<P *>(this);
    }
    P &succeed(T &&result)
    {
      this->result = result;
      return *static_cast<P *>(this);
    }
  };

  // partial specialization for void result
  template <typename P>
  struct _ParseResult<P, void>
  {
  };

  // Actual ParseResult class
  template <typename T>
  struct ParseResult : public _ParseResult<ParseResult<T>, T>
  {
    const char *next = NULL;
    const char *err = NULL;
    const char *ctx = NULL;

    ParseResult &fail(const char *err, const char *ctx = NULL)
    {
      this->err = err;
      this->ctx = ctx;
      return *this;
    }
    ParseResult &until(const char *next)
    {
      this->next = next;
      return *this;
    }
    ParseResult() = default;
    ParseResult(const ParseResult &other) = default;

    template <typename T2>
    ParseResult(const ParseResult<T2> &other) : next(other.next), err(other.err), ctx(other.ctx) {}

    /**
   * Returns the error, including context in a fancy multi-line format.
   * The start and end passed are the first and one-past-the-end
   * characters in the total parsed string. These are needed to properly
   * limit the context output.
   */
    String fullError(const char *start, const char *end) const
    {
      String res;
      if (this->ctx && start && end)
      {
        // Find the entire line surrounding the context
        const char *line_end = this->ctx;
        while (line_end < end && line_end[0] != '\r' && line_end[0] != '\n')
          ++line_end;
        const char *line_start = this->ctx;
        while (line_start > start && line_start[-1] != '\r' && line_start[-1] != '\n')
          --line_start;

        // We can now predict the context string length, so let String allocate
        // memory in advance
        res.reserve((line_end - line_start) + 2 + (this->ctx - line_start) + 1 + 2);

        // Write the line
        concat_hack(res, line_start, line_end - line_start);
        res += "\r\n";

        // Write a marker to point out ctx
        while (line_start++ < this->ctx)
          res += ' ';
        res += '^';
        res += "\r\n";
      }
      res += this->err;
      return res;
    }
  };

  /**
 * An OBIS id is 6 bytes, usually noted as a-b:c.d.e.f. Here we put them
 * in an array for easy parsing.
 */
  struct ObisId
  {
    uint8_t v[6];

    constexpr ObisId(uint8_t a, uint8_t b = 255, uint8_t c = 255, uint8_t d = 255, uint8_t e = 255, uint8_t f = 255)
        : v{a, b, c, d, e, f} {};
    constexpr ObisId() : v() {} // Zeroes

    bool operator==(const ObisId &other) const { return memcmp(&v, &other.v, sizeof(v)) == 0; }
  };

} // namespace dsmr
