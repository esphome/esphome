#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace esphome::climate_ir_panasonic {

/*
Panasonic A/C uses a 27 byte (216 bit) state, sent as two sections (8 bytes + 19 bytes).

The protocol description (state layout, timings, checksum and the per-model bytes) is based on the work of
Kristian Lauszus and David Conran in IRremoteESP8266 (LGPL-2.1):
https://github.com/crankyoldgit/IRremoteESP8266/blob/master/src/ir_Panasonic.cpp

Cross-checked against the "Panasonic HVAC IR Protocol" reverse engineering notes by Mathieu:
https://github.com/r45635/HVAC-IR-Control/blob/master/Protocol/Panasonic%20HVAC%20IR%20Protocol%20specification.pdf

and the earlier write-up of the same work (a different remote, with a declarative power bit):
https://www.instructables.com/Reverse-engineering-of-an-Air-Conditioning-control/

Checked against the frames in IRremoteESP8266's unit tests, the example frames from the two sources above, and
captures from the remote of the author's own CS-NE9PKE heat pump: heat, cool, dry, fan only and auto, every fan
speed, on and off, the vane positions and the 8/10 C button. Its quiet/powerful and nanoe-G buttons are short frames
(see below).

B0-B12:
  Static: 0x0220E004000000060220E00400
B13:
  b0:   Power (CKP: toggle)
  b1:   On timer enabled (B18-B19)
  b2:   Off timer enabled (B19-B20)
  b3:   Not understood. Set by RKR in IRremoteESP8266, and in every frame from the remote of a CS-NE9PKE (0x49).
  IRremoteESP8266 clears b1-b3 for all models, except LKE, which always has b1 set, and RKR, which has b3 set.
  Real captures from CKP and DKE remotes have the timer flags set (0x4E, 0x4F), JKE has them cleared.
  The timers aren't used by this component, so we do the same as IRremoteESP8266.
  b4-6: Mode
B14:
  b1-5: Temperature set-point
B16:
  b0-3: Vertical vane position: 1 (highest) to 5 (lowest), or 15 for auto
  b4-7: Fan speed (+3, so min = 3 ... max = 7, auto = 10)
B17:
  b0-3: Horizontal vane position (model specific)
B18:
  OnTimer minutes_since_midnight (round down to nearest 10min)
  Max value is 23h and 59min.
  A value of 0x600 has some special meaning. Unclear
B19:
  b0-2: 3 MSb of time from above.
  b4-7: 4 LSb of the Offtimer
B20:
  b0-6: 7 MSb of the Offtimer
  (The timers are not set by this component. B18-B20 are left at the template values)
B21:
  Default: b0: quiet, b5: powerful. (CKP, RKR: swapped)
  CKP:     b4: constant 1
B23:
  Varies between models
B24:
  Time in minutes since midnight (not rounded down)
  Max value is 23h and 59min.
  A value of 0x600 has some special meaning. Unclear
B25:
  b0-2: 3 MSb of time from above.
  (The clock is not set by this component. B24-B25 are left at the template values.
  Some models use B25 for something else, DKE has 0x06 here)
Besides this 27 byte state, remotes also send short frames for stateless buttons (powerful/quiet, nanoe-G), where the
second section is just 8 bytes: 02 20 E0 04 + 3 bytes + checksum (sum of the 7 previous bytes). Those are ignored here.
On the remote of a CS-NE9PKE, quiet and powerful are one button (80 9C 32) and nanoe-G another (80 61 33). Neither frame
says whether the feature is being switched on or off.

B26:
  Checksum. Sum of all previous bytes, plus 0xF4.
  The first section (B0-B7) sums to 0x10C, so this is the same as the sum of B8-B25 alone
*/

static constexpr size_t STATE_LENGTH = 27;
static constexpr size_t SECTION1_LENGTH = 8;
static constexpr size_t SECTION2_LENGTH = STATE_LENGTH - SECTION1_LENGTH;
static constexpr uint8_t CHECKSUM_INIT = 0xF4;

// PKE is not an IRremoteESP8266 model. It is what the remote of a CS-NE9PKE heat pump sends: JKE, with B13 b3 set.
// The service manual lists that remote as part CWA75C4410. It is the remote of the author's own unit, which is used
// for testing. It is not known if other remotes send this.
enum class Model : uint8_t { DKE, JKE, LKE, NKE, CKP, RKR, PKE };

enum class Mode : uint8_t { AUTO = 0, DRY = 2, COOL = 3, HEAT = 4, FAN = 6 };

// Raw fan speeds, as used by IRremoteESP8266. 0 = minimum, 4 = maximum.
static constexpr uint8_t FAN_SPEED_MIN = 0;
static constexpr uint8_t FAN_SPEED_LOW = 1;
static constexpr uint8_t FAN_SPEED_MEDIUM = 2;
static constexpr uint8_t FAN_SPEED_HIGH = 3;
static constexpr uint8_t FAN_SPEED_MAX = 4;
static constexpr uint8_t FAN_SPEED_AUTO = 7;
static constexpr uint8_t FAN_SPEED_OFFSET = 3;

// Fixed vane positions are 1 (highest) to 5 (lowest)
static constexpr uint8_t VANE_V_HIGHEST = 0x1;
static constexpr uint8_t VANE_V_MIDDLE = 0x3;
static constexpr uint8_t VANE_V_LOWEST = 0x5;
static constexpr uint8_t VANE_V_AUTO = 0xF;

static constexpr uint8_t VANE_H_MIDDLE = 0x6;

// Frost protection ("maintenance") of the remote of a CS-NE9PKE: the +8/10 C Heat button switches to a mode of its
// own, and in it the remote only offers 8 C and 10 C as the temperature. Both are understood when receiving, 10 C is
// what we send. It's a mode, not a set-point of the normal range.
static constexpr uint8_t TEMP_MAINTENANCE = 10;
static constexpr uint8_t TEMP_MIN_RECEIVE = 8;
static constexpr uint8_t TEMP_MIN = 16;       // Celsius
static constexpr uint8_t TEMP_MAX = 30;       // Celsius
static constexpr uint8_t TEMP_FAN_MODE = 27;  // Celsius. The remote always sends this in fan mode

struct FrameParams {
  Model model{Model::JKE};
  bool power{false};  // For CKP this means "toggle power"
  Mode mode{Mode::AUTO};
  uint8_t temperature{25};
  uint8_t fan_speed{FAN_SPEED_AUTO};
  uint8_t vane_v{VANE_V_AUTO};
  uint8_t vane_h{VANE_H_MIDDLE};
  bool quiet{false};
  bool powerful{false};
  /// The remote's frost protection mode: heat at 10 C (see TEMP_MAINTENANCE), with the fan at maximum.
  /// It's a mode of its own, not a temperature you can reach by turning the set-point down.
  /// When set, `mode`, `temperature`, `fan_speed` and `vane_v` are overridden when encoding.
  bool maintenance{false};
};

/// The part of the state that doesn't depend on the settings. Built in code, so it doesn't take up RAM on an ESP8266
inline void fill_template(uint8_t *state) {
  memset(state, 0, STATE_LENGTH);
  state[0] = 0x02;
  state[1] = 0x20;
  state[2] = 0xE0;
  state[3] = 0x04;
  state[7] = 0x06;
  state[8] = 0x02;
  state[9] = 0x20;
  state[10] = 0xE0;
  state[11] = 0x04;
  state[15] = 0x80;
  state[19] = 0x0E;
  state[20] = 0xE0;
  state[23] = 0x81;
}

inline uint8_t calc_checksum(const uint8_t *state, size_t len) {
  uint8_t sum = CHECKSUM_INIT;
  for (size_t i = 0; i < len - 1; i++)
    sum += state[i];
  return sum;
}

inline void encode_frame(const FrameParams &params, uint8_t *state) {
  FrameParams p = params;
  if (p.maintenance) {
    p.mode = Mode::HEAT;
    p.temperature = TEMP_MAINTENANCE;
    p.fan_speed = FAN_SPEED_MAX;
    p.vane_v = VANE_V_AUTO;
  }
  fill_template(state);

  // Model specific bytes
  bool swap_quiet_powerful = false;
  switch (p.model) {
    case Model::LKE:
      state[13] |= 0x02;
      state[17] = 0x06;
      break;
    case Model::DKE:
      state[17] = p.vane_h & 0x0F;
      state[23] = 0x01;
      state[25] = 0x06;
      break;
    case Model::NKE:
      state[17] = 0x06;
      break;
    case Model::JKE:
      break;
    case Model::PKE:
      state[13] |= 0x08;
      break;
    case Model::CKP:
      state[21] |= 0x10;
      state[23] = 0x01;
      swap_quiet_powerful = true;
      break;
    case Model::RKR:
      state[13] |= 0x08;
      state[17] = p.vane_h & 0x0F;
      state[23] = 0x89;
      swap_quiet_powerful = true;
      break;
  }

  // Power and mode
  if (p.power)
    state[13] |= 0x01;
  state[13] |= (static_cast<uint8_t>(p.mode) & 0x07) << 4;

  // Temperature
  uint8_t temp = p.temperature;
  if (p.mode == Mode::FAN) {
    temp = TEMP_FAN_MODE;
  } else if (temp < TEMP_MIN && !p.maintenance) {
    temp = TEMP_MIN;
  } else if (temp > TEMP_MAX) {
    temp = TEMP_MAX;
  }
  state[14] |= (temp & 0x1F) << 1;

  // Vane (low nibble) and fan (high nibble)
  state[16] = (p.vane_v & 0x0F) | (((p.fan_speed + FAN_SPEED_OFFSET) & 0x0F) << 4);

  // Quiet and powerful are mutually exclusive. Powerful wins.
  const uint8_t quiet_bit = swap_quiet_powerful ? 0x20 : 0x01;
  const uint8_t powerful_bit = swap_quiet_powerful ? 0x01 : 0x20;
  if (p.powerful) {
    state[21] |= powerful_bit;
  } else if (p.quiet) {
    state[21] |= quiet_bit;
  }

  state[STATE_LENGTH - 1] = calc_checksum(state, STATE_LENGTH);
}

/// Decode a state. `p.model` must be set by the caller, as it decides how quiet/powerful are laid out.
/// Returns false if the state doesn't look like a valid Panasonic A/C state.
inline bool decode_frame(const uint8_t *state, FrameParams &p) {
  // The first 13 bytes are static
  uint8_t expected[STATE_LENGTH];
  fill_template(expected);
  if (memcmp(state, expected, 13) != 0)
    return false;
  if (state[STATE_LENGTH - 1] != calc_checksum(state, STATE_LENGTH))
    return false;

  const uint8_t mode = (state[13] >> 4) & 0x07;
  switch (static_cast<Mode>(mode)) {
    case Mode::AUTO:
    case Mode::DRY:
    case Mode::COOL:
    case Mode::HEAT:
    case Mode::FAN:
      break;
    default:
      return false;
  }

  const uint8_t temp = (state[14] >> 1) & 0x1F;
  if (temp < TEMP_MIN_RECEIVE || temp > TEMP_MAX)
    return false;

  const uint8_t fan = state[16] >> 4;
  if (fan < FAN_SPEED_OFFSET + FAN_SPEED_MIN || fan > FAN_SPEED_OFFSET + FAN_SPEED_MAX) {
    if (fan != FAN_SPEED_AUTO + FAN_SPEED_OFFSET)
      return false;
  }

  const bool swap_quiet_powerful = p.model == Model::CKP || p.model == Model::RKR;
  const uint8_t quiet_bit = swap_quiet_powerful ? 0x20 : 0x01;
  const uint8_t powerful_bit = swap_quiet_powerful ? 0x01 : 0x20;

  p.power = state[13] & 0x01;
  p.mode = static_cast<Mode>(mode);
  p.maintenance = temp < TEMP_MIN;
  p.temperature = temp;
  p.fan_speed = fan - FAN_SPEED_OFFSET;
  p.vane_v = state[16] & 0x0F;
  p.vane_h = state[17] & 0x0F;
  p.quiet = state[21] & quiet_bit;
  p.powerful = state[21] & powerful_bit;
  return true;
}

// IR timings [us]
static constexpr uint32_t CARRIER_FREQUENCY = 36700;
static constexpr uint32_t HEADER_MARK = 3456;
static constexpr uint32_t HEADER_SPACE = 1728;
static constexpr uint32_t BIT_MARK = 432;
static constexpr uint32_t BIT_ONE_SPACE = 1296;
static constexpr uint32_t BIT_ZERO_SPACE = 432;
static constexpr uint32_t SECTION_GAP = 10000;

/// Reads the footer mark of a section. It's followed by either the end of the capture, or a long space (the receiver's
/// idle time, or the gap to the next section), but never by another bit.
template<typename Data> bool receive_footer(Data &data) {
  if (!data.expect_mark(BIT_MARK))
    return false;
  return !data.is_valid() || data.peek_space_at_least(3 * BIT_ONE_SPACE);
}

/// Reads header + bytes + footer mark. Returns the number of bytes read, or 0 on error.
/// `Data` is a remote_base::RemoteReceiveData (or something that behaves like it).
template<typename Data> size_t receive_section(Data &data, uint8_t *bytes, size_t max_len) {
  if (!data.expect_item(HEADER_MARK, HEADER_SPACE))
    return 0;

  size_t len = 0;
  while (true) {
    if (len == max_len)
      return receive_footer(data) ? len : 0;  // Footer, or too long
    uint8_t byte = 0;
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (data.expect_item(BIT_MARK, BIT_ONE_SPACE)) {
        byte |= 1 << bit;
      } else if (!data.expect_item(BIT_MARK, BIT_ZERO_SPACE)) {
        // Anything that isn't a bit is only valid between bytes, where it ends the section
        if (bit != 0)
          return 0;
        return receive_footer(data) ? len : 0;
      }
    }
    bytes[len++] = byte;
  }
}

/// Reads a state from a received capture. Returns true if `state` holds a complete state (not yet validated).
///
/// A capture holds either the whole frame, or just one of its sections, depending on the receiver's idle time:
/// the 10 ms gap between the sections is as long as the default idle time, and a shorter idle time always splits
/// them. The first section is a constant, and the checksum only covers the second one, so all the information
/// is in the second section. A lone first section is ignored.
template<typename Data> bool read_state(Data &data, uint8_t *state) {
  uint8_t section[SECTION2_LENGTH];
  size_t len = receive_section(data, section, SECTION2_LENGTH);

  uint8_t expected[STATE_LENGTH];
  fill_template(expected);

  if (len == SECTION1_LENGTH) {
    if (memcmp(section, expected, SECTION1_LENGTH) != 0)
      return false;
    // The gap and the second section follow in a full capture. Anything else is just the receiver's idle space
    if (!data.is_valid(1) || !data.expect_space(SECTION_GAP))
      return false;
    len = receive_section(data, section, SECTION2_LENGTH);
  }
  if (len != SECTION2_LENGTH)
    return false;

  memcpy(state, expected, SECTION1_LENGTH);
  memcpy(state + SECTION1_LENGTH, section, SECTION2_LENGTH);
  return true;
}

}  // namespace esphome::climate_ir_panasonic
