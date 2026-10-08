#include "hob2hood_protocol.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

#include <array>
#include <cstdlib>

namespace esphome::remote_base {

static const char *const TAG = "remote.hob2hood";

// A frame is 25 bits: a leading 0, then the command byte, command + 1 and command + 2. Zero bits are marks
// and one bits are spaces; equal neighbours merge into one run of n * BIT_TIME_US plus a fixed adjustment.
static constexpr uint8_t NBITS = 25;
static constexpr uint32_t BIT_TIME_US = 700;
static constexpr int32_t MARK_ADJUST_US = 300;
static constexpr int32_t SPACE_ADJUST_US = -200;
// The longest frame (light_off) has 18 runs
static constexpr uint8_t MAX_RUNS = 18;

static constexpr std::array<Hob2HoodCommand, 7> COMMANDS = {
    HOB2HOOD_COMMAND_LIGHT_OFF,  HOB2HOOD_COMMAND_LIGHT_ON, HOB2HOOD_COMMAND_FAN_OFF, HOB2HOOD_COMMAND_FAN_LOW,
    HOB2HOOD_COMMAND_FAN_MEDIUM, HOB2HOOD_COMMAND_FAN_HIGH, HOB2HOOD_COMMAND_FAN_MAX,
};
// Same order as COMMANDS; the last entry is the fallback
PROGMEM_STRING_TABLE(Hob2HoodCommandNames, "light_off", "light_on", "fan_off", "fan_low", "fan_medium", "fan_high",
                     "fan_max", "unknown");

// Walks the frame of `command` as runs of equal bits. emit(is_mark, length_us, is_last) returns false to stop.
template<typename F> static bool walk_runs(Hob2HoodCommand command, F &&emit) {
  // Shifted so the first of the 25 bits is the top bit
  uint32_t bits = ((uint32_t(command) << 16) | (uint32_t(uint8_t(command + 1)) << 8) | uint8_t(command + 2))
                  << (32 - NBITS);
  int8_t run = 0;
  for (uint8_t i = 0; i < NBITS; i++, bits <<= 1) {
    const bool bit = (bits & 0x80000000) != 0;
    run += bit ? 1 : -1;
    const bool last = i == NBITS - 1;
    if (last || (((bits << 1) & 0x80000000) != 0) != bit) {
      const uint32_t length = BIT_TIME_US * std::abs(run) + (run < 0 ? MARK_ADJUST_US : SPACE_ADJUST_US);
      if (!emit(run < 0, length, last))
        return false;
      run = 0;
    }
  }
  return true;
}

void Hob2HoodProtocol::encode(RemoteTransmitData *dst, const Hob2HoodData &data) {
  dst->set_carrier_frequency(38000);
  dst->reserve(MAX_RUNS);
  walk_runs(data.command, [dst](bool is_mark, uint32_t length, bool) {
    if (is_mark) {
      dst->mark(length);
    } else {
      dst->space(length);
    }
    return true;
  });
}

optional<Hob2HoodData> Hob2HoodProtocol::decode(RemoteReceiveData src) {
  for (auto command : COMMANDS) {
    src.reset();
    // The receiver does not capture a trailing space, so the last run only has to match when it is a mark
    const bool matched = walk_runs(command, [&src](bool is_mark, uint32_t length, bool last) {
      return is_mark ? src.expect_mark(length) : (last || src.expect_space(length));
    });
    if (matched)
      return Hob2HoodData{command};
  }
  return {};
}

void Hob2HoodProtocol::dump(const Hob2HoodData &data) {
  uint8_t index = 0;
  while (index < COMMANDS.size() && COMMANDS[index] != data.command)
    index++;
  ESP_LOGI(TAG, "Received Hob2Hood: %s",
           LOG_STR_ARG(Hob2HoodCommandNames::get_log_str(index, Hob2HoodCommandNames::LAST_INDEX)));
}

}  // namespace esphome::remote_base
