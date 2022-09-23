#include "mr24hpb1.h"

namespace esphome {
namespace mr24hpb1 {

static const int PACKET_START = 0x55;

static const uint16_t PACKET_WAIT_TIMEOUT_MS = 5000;

// Read Commands
static const FunctionCode READ_COMMAND = 0x01;

static const AddressCode1 RC_MARKING_SEARCH = 0x01;
static const AddressCode2 RC_MS_DEVICE_ID = 0x01;
static const AddressCode2 RC_MS_SOFTWARE_VERSION = 0x02;
static const AddressCode2 RC_MS_HARDWARE_VERSION = 0x03;
static const AddressCode2 RC_MS_PROTOCOL_VERSION = 0x04;

static const AddressCode1 RC_RADAR_INFORMATION_SEARCH = 0x03;
static const AddressCode2 RC_RIS_ENVIRONMENTAL_STATUS = 0x05;
static const AddressCode2 RC_RIS_SIGNS_PARAMETERS = 0x06;

static const AddressCode1 RC_SYSTEM_PARAMETER_SEARCH = 0x04;
static const AddressCode2 RC_SPS_THRESHOLD_GEAR = 0x0C;
static const AddressCode2 RC_SPS_SCENE_SETTING = 0x10;

static const AddressCode1 RC_OTHER_INFORMATION = 0x05;
// Enquiry current fall function switch.
static const AddressCode2 RC_OI_FALL_FUNCTION_SWITCH = 0x0B;
// Enquiry current fall alarm time.
static const AddressCode2 RC_OI_FALL_ALARM_TIME = 0x0C;
// Enquiry current fall sensitivity.
static const AddressCode2 RC_OI_FALL_SENSITIVITY = 0x0E;

// Copy order
static const FunctionCode COPY_ORDER = 0x02;

static const AddressCode1 CO_SYSTEM_PARAMETERS = 0x04;
/**
 * Threshold gear.
 *
 * Gears 1 - 10 (default: 7).
 * The higher the gear, the more sensitive it is.
 */
static const AddressCode2 CO_SP_THRESHOLD_GEAR = 0x0C;
/**
 * Scene setting.
 *
 * Use a value from enum SceneSetting.
 */
static const AddressCode2 CO_SP_SCENE_SETTING = 0x10;
/**
 * Forced into unoccupied
 *
 * Use a value from enum ForcedUnoccupied.
 */
static const AddressCode2 CO_SP_FORCED_UNOCCUPIED_SETTING = 0x12;

static const AddressCode1 CO_OTHER_FUNCTIONS = 0x05;
static const AddressCode2 CO_OF_REBOOT = 0x04;
/**
 * Fall function switch.
 *
 * Use a value from enum FallFunctionSwitch.
 */
static const AddressCode2 CO_OF_FALL_FUNCTION_SWITCH = 0x0B;
/**
 * Fall alarm time.
 *
 * Use a value from enum FallAlarmTime.
 */
static const AddressCode2 CO_OF_FALL_ALARM_TIME = 0x0C;
/**
 * Fall sensitivity setting.
 *
 * Gears 1 - 10 (default: 4).
 * The lower the gear, the less sensitive it is.
 * The higher the gear, the more sensitive it is.
 */
static const AddressCode2 CO_OF_FALL_SENSITIVITY_SETTING = 0x0E;
/**
 * Start OTA upgrade.
 *
 * For the love of god and all that is special to us:
 * DO NOT - i repeat - DO NOT USE!
 */
static const AddressCode2 CO_OF_START_OTA_UPGRADE = 0x08;
static const AddressCode2 CO_OF_UPGRADE_PACKAGE_TRANSFER = 0x09;
static const AddressCode2 CO_OF_END_OF_UPGRADE_INFORMATION = 0x0A;

// Passive reporting of orders
static const FunctionCode PASSIVE_REPORTING = 0x03;

static const AddressCode1 PR_REPORTING_MODULE_ID = 0x01;
/**
 * Device ID.
 *
 * 12 bytes of data.
 */
static const AddressCode2 PR_RMI_DEVICE_ID = 0x01;
/**
 * Software version.
 *
 * 10 bytes of data.
 */
static const AddressCode2 PR_RMI_SOFTWARE_VERSION = 0x02;
/**
 * Hardware version.
 *
 * 8 bytes of data.
 */
static const AddressCode2 PR_RMI_HARDWARE_VERSION = 0x03;
/**
 * Protocol version.
 *
 * 8 bytes of data.
 */
static const AddressCode2 PR_RMI_PROTOCOL_VERSION = 0x04;

static const AddressCode1 PR_REPORT_RADAR_INFO = 0x03;

static const AddressCode1 PR_REPORTING_SYSTEM_INFO = 0x04;

static const AddressCode2 PR_RSI_THRESHOLD_GEAR = 0x0C;
static const AddressCode2 PR_RSI_SCENE_SETTINGS = 0x10;
static const AddressCode2 PR_RSI_FORCED_UNOCCUPIED_SETTINGS = 0x12;

static const AddressCode1 PR_OTHER_FUNCTIONS = 0x05;

static const AddressCode2 PR_OTH_SLEEP_FUNCTION_SWITCH = 0x0D;
static const AddressCode2 PR_OTH_FALL_FUNCTION_SWITCH = 0x0B;
static const AddressCode2 PR_OTH_FALL_ALARM_TIME = 0x0C;
static const AddressCode2 PR_OTH_FALL_SENSITIVITY_SETTING = 0x0E;
static const AddressCode2 PR_OTH_START_OTA_UPGRADE = 0x08;
static const AddressCode2 PR_OTH_UPGRADE_PACKAGE = 0x09;

// Proactive reporting of commands
static const FunctionCode PROACTIVE_REPORTING = 0x04;

static const AddressCode1 PROREP_REPORTING_MODULE_ID = 0x01;
// TODO: address code 2 like PR

static const AddressCode1 PROREP_REPORT_RADAR_INFO = 0x03;
static const AddressCode2 PROREP_RRI_ENVIRONMENT_STATUS = 0x05;
static const AddressCode2 PROREP_RRI_MOVEMENT_SIGNS_PARAMETERS = 0x06;
static const AddressCode2 PROREP_RRI_APPROACHING_AWAY_STATE = 0x07;

static const AddressCode1 PROREP_REPORT_OTHER_INFORMATION = 0x05;
static const AddressCode2 PROREP_ROI_HEARTBEAT_PACK = 0x01;
/**
 * Abnormal reset.
 *
 * 1 byte of data (always 0x0F).
 */
static const AddressCode2 PROREP_ROI_ABNORMAL_RESET = 0x02;

/**
 * Initialization sucessful
 *
 * 1 byte of data (always 0x0F).
 */
static const AddressCode2 PROREP_ROI_INIT_SUCCESSFUL = 0x0A;

// Sleeping data reporting
static const FunctionCode SLEEPING_DATA_REPORTING = 0x05;

static const AddressCode1 SLP_BREATHING_REPORTING_PARAMETERS = 0x01;
static const AddressCode2 SLP_BREA_BREATHING_RATE = 0x01;
static const AddressCode2 SLP_BREA_DETECTION_SIGNAL = 0x06;

static const AddressCode1 SLP_SCENARIO_ASSESMENT = 0x03;
static const AddressCode2 SLP_SCEN_BED_OCCUPATION = 0x07;
static const AddressCode2 SLP_SCEN_SLEEP_STATE_ASSESMENT = 0x07;  // unsure, hex code is missing from documentation

static const AddressCode1 SLP_SLEEP_DURATIONS = 0x04;
static const AddressCode2 SLP_SLE_DURATION_WAKEFUL = 0x01;
static const AddressCode2 SLP_SLE_DURATION_LIGHT_SLEEP = 0x02;
static const AddressCode2 SLP_SLE_DURATION_DEEP_SLEEP = 0x03;

static const AddressCode1 SLP_SLEEP_QUALITY_PARAMETER = 0x05;
static const AddressCode2 SLP_SLEQ_SLEEP_QUALITY_SCORE = 0x01;

static const AddressCode1 SLP_HEART_RATE_PARAMETER = 0x06;
static const AddressCode2 SLP_HEAR_HEART_RATE = 0x01;

// Fall detection data reporting
static const FunctionCode FALL_DETECTION_DATA_REPORTING = 0x06;
static const AddressCode1 FAL_ALARM = 0x01;
static const AddressCode2 FAL_AL_FALL_ALARM = 0x01;
static const AddressCode2 FAL_AL_DWELL_ALARM = 0x02;

enum class FallFunctionSwitch { OFF = 0x00, ON = 0x01 };

enum class FallAlarmTime {
  MINUTES_1 = 0x00,
  MINUTES_2 = 0x01,
  MINUTES_3 = 0x02,
  MINUTES_4 = 0x03,
  MINUTES_5 = 0x04,
  MINUTES_6 = 0x05,
  MINUTES_7 = 0x06,
  MINUTES_10 = 0x07,
  MINUTES_15 = 0x08,
  MINUTES_30 = 0x09
};

}  // namespace mr24hpb1
}  // namespace esphome