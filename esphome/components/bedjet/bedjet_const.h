#pragma once

#include <set>

namespace esphome {
namespace bedjet {

static const char *const TAG = "bedjet";

/// Converts a BedJet fan step to a speed percentage, in the range of 5% to 100%.
inline static uint8_t bedjet_fan_step_to_speed(const uint8_t fan) {
  //  0 =  5%
  // 19 = 100%
  return 5 * fan + 5;
}
inline static uint8_t bedjet_fan_speed_to_index(const uint8_t speed) { return speed / 5 - 1; }

enum BedjetMode : uint8_t {
  /// BedJet is Off
  MODE_STANDBY = 0,
  /// BedJet is in Heat mode (limited to 4 hours)
  MODE_HEAT = 1,
  /// BedJet is in Turbo mode (high heat, limited time)
  MODE_TURBO = 2,
  /// BedJet is in Extended Heat mode (limited to 10 hours)
  MODE_EXTHT = 3,
  /// BedJet is in Cool mode (actually "Fan only" mode)
  MODE_COOL = 4,
  /// BedJet is in Dry mode (high speed, no heat)
  MODE_DRY = 5,
  /// BedJet is in "wait" mode, a step during a biorhythm program
  MODE_WAIT = 6,
};

/** Optional heating strategies to use for climate::CLIMATE_MODE_HEAT. */
enum BedjetHeatMode {
  /// HVACMode.HEAT is handled using BTN_HEAT (default)
  HEAT_MODE_HEAT,
  /// HVACMode.HEAT is handled using BTN_EXTHT
  HEAT_MODE_EXTENDED,
};

enum BedjetButton : uint8_t {
  /// Turn BedJet off
  BTN_OFF = 0x1,
  /// Enter Cool mode (fan only)
  BTN_COOL = 0x2,
  /// Enter Heat mode (limited to 4 hours)
  BTN_HEAT = 0x3,
  /// Enter Turbo mode (high heat, limited to 10 minutes)
  BTN_TURBO = 0x4,
  /// Enter Dry mode (high speed, no heat)
  BTN_DRY = 0x5,
  /// Enter Extended Heat mode (limited to 10 hours)
  BTN_EXTHT = 0x6,

  /// Start the M1 biorhythm/preset program
  BTN_M1 = 0x20,
  /// Start the M2 biorhythm/preset program
  BTN_M2 = 0x21,
  /// Start the M3 biorhythm/preset program
  BTN_M3 = 0x22,

  /* These are "MAGIC" buttons */

  /// Turn debug mode on/off
  MAGIC_DEBUG_ON = 0x40,
  MAGIC_DEBUG_OFF = 0x41,
  /// Perform a connection test.
  MAGIC_CONNTEST = 0x42,
  /// Request a firmware update. This will also restart the Bedjet.
  MAGIC_UPDATE = 0x43,
  /// Acknowledge notification handled. See BedjetNotify
  MAGIC_NOTIFY_ACK = 0x52,
};

enum BedjetCommand : uint8_t {
  CMD_BUTTON = 0x1,
  CMD_SET_RUNTIME = 0x2,
  CMD_SET_TEMP = 0x3,
  CMD_STATUS = 0x6,
  CMD_SET_FAN = 0x7,
  CMD_SET_CLOCK = 0x8,
};

#define BEDJET_FAN_STEP_NAMES_ \
  { \
    "5%", "10%", "15%", "20%", "25%", "30%", "35%", "40%", "45%", "50%", "55%", "60%", "65%", "70%", "75%", "80%", \
        "85%", "90%", "95%", "100%" \
  }

static const uint8_t BEDJET_FAN_SPEED_COUNT = 20;

static const char *const BEDJET_FAN_STEP_NAMES[BEDJET_FAN_SPEED_COUNT] = BEDJET_FAN_STEP_NAMES_;
static const std::string BEDJET_FAN_STEP_NAME_STRINGS[BEDJET_FAN_SPEED_COUNT] = BEDJET_FAN_STEP_NAMES_;
static const std::set<std::string> BEDJET_FAN_STEP_NAMES_SET BEDJET_FAN_STEP_NAMES_;

}  // namespace bedjet
}  // namespace esphome
