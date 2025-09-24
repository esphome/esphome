#include "rc6_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const RC6_TAG = "remote.rc6";

static const uint16_t RC6_FREQ = 36000;
static const uint16_t RC6_UNIT = 444;
static const uint16_t RC6_HEADER_MARK = (6 * RC6_UNIT);
static const uint16_t RC6_HEADER_SPACE = (2 * RC6_UNIT);
static const uint16_t RC6_MODE_MASK = 0x07;

void RC6Protocol::encode(RemoteTransmitData *dst, const RC6Data &data) {
  dst->reserve(44);
  dst->set_carrier_frequency(RC6_FREQ);

  // Encode header
  dst->item(RC6_HEADER_MARK, RC6_HEADER_SPACE);

  int32_t next{0};

  // Encode startbit+mode
  uint8_t header{static_cast<uint8_t>((1 << 3) | data.mode)};

  for (uint8_t mask = 0x8; mask; mask >>= 1) {
    if (header & mask) {
      if (next < 0) {
        dst->space(-next);
        next = 0;
      }
      if (next >= 0) {
        next = next + RC6_UNIT;
        dst->mark(next);
        next = -RC6_UNIT;
      }
    } else {
      if (next > 0) {
        dst->mark(next);
        next = 0;
      }
      if (next <= 0) {
        next = next - RC6_UNIT;
        dst->space(-next);
        next = RC6_UNIT;
      }
    }
  }

  // Toggle
  if (data.toggle) {
    if (next < 0) {
      dst->space(-next);
      next = 0;
    }
    if (next >= 0) {
      next = next + RC6_UNIT * 2;
      dst->mark(next);
      next = -RC6_UNIT * 2;
    }
  } else {
    if (next > 0) {
      dst->mark(next);
      next = 0;
    }
    if (next <= 0) {
      next = next - RC6_UNIT * 2;
      dst->space(-next);
      next = RC6_UNIT * 2;
    }
  }

  // Encode data
  uint16_t raw{static_cast<uint16_t>((data.address << 8) | data.command)};

  for (uint16_t mask = 0x8000; mask; mask >>= 1) {
    if (raw & mask) {
      if (next < 0) {
        dst->space(-next);
        next = 0;
      }
      if (next >= 0) {
        next = next + RC6_UNIT;
        dst->mark(next);
        next = -RC6_UNIT;
      }
    } else {
      if (next > 0) {
        dst->mark(next);
        next = 0;
      }
      if (next <= 0) {
        next = next - RC6_UNIT;
        dst->space(-next);
        next = RC6_UNIT;
      }
    }
  }

  if (next > 0) {
    dst->mark(next);
  } else {
    dst->space(-next);
  }
}

optional<RC6Data> RC6Protocol::decode(RemoteReceiveData src) {
  RC6Data data{
      .mode = 0,
      .toggle = 0,
      .address = 0,
      .command = 0,
  };

  // Check if header matches
  if (!src.expect_item(RC6_HEADER_MARK, RC6_HEADER_SPACE)) {
    return {};
  }

  uint8_t bit{1};
  uint8_t offset{0};
  uint8_t header{0};
  uint32_t buffer{0};

  // Startbit + mode
  while (offset < 4) {
    bit = src.peek() > 0;
    header = header + (bit << (3 - offset++));
    src.advance();

    if (src.peek_mark(RC6_UNIT) || src.peek_space(RC6_UNIT)) {
      src.advance();
    } else if (offset == 4) {
      break;
    } else if (!src.peek_mark(RC6_UNIT * 2) && !src.peek_space(RC6_UNIT * 2)) {
      return {};
    }
  }

  data.mode = header & RC6_MODE_MASK;

  if (data.mode != 0) {
    return {};  // I dont have a device to test other modes
  }

  // Toggle
  data.toggle = src.peek() > 0;
  src.advance();
  if (src.peek_mark(RC6_UNIT * 2) || src.peek_space(RC6_UNIT * 2)) {
    src.advance();
  }

  // Data
  offset = 0;
  while (offset < 16) {
    bit = src.peek() > 0;
    buffer = buffer + (bit << (15 - offset++));
    src.advance();

    if (offset == 16) {
      break;
    } else if (src.peek_mark(RC6_UNIT) || src.peek_space(RC6_UNIT)) {
      src.advance();
    } else if (!src.peek_mark(RC6_UNIT * 2) && !src.peek_space(RC6_UNIT * 2)) {
      return {};
    }
  }

  data.address = (0xFF00 & buffer) >> 8;
  data.command = (0x00FF & buffer);
  return data;
}

void RC6Protocol::dump(const RC6Data &data) {
  ESP_LOGI(RC6_TAG, "Received RC6: mode=0x%X, address=0x%02X, command=0x%02X, toggle=0x%X", data.mode, data.address,
           data.command, data.toggle);
}

}  // namespace remote_base
}  // namespace esphome
