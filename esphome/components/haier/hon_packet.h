#pragma once

#include <cstdint>

namespace esphome {
namespace haier {
namespace hon_protocol {

enum class VerticalSwingMode : uint8_t {
  HEALTH_UP = 0x01,
  MAX_UP = 0x02,
  HEALTH_DOWN = 0x03,
  UP = 0x04,
  CENTER = 0x06,
  DOWN = 0x08,
  MAX_DOWN = 0x0A,
  AUTO = 0x0C,
  // Auto for special modes
  AUTO_SPECIAL = 0x0E
};

enum class HorizontalSwingMode : uint8_t {
  CENTER = 0x00,
  MAX_LEFT = 0x03,
  LEFT = 0x04,
  RIGHT = 0x05,
  MAX_RIGHT = 0x06,
  AUTO = 0x07
};

enum class ConditioningMode : uint8_t {
  AUTO = 0x00,
  COOL = 0x01,
  DRY = 0x02,
  HEALTHY_DRY = 0x03,
  HEAT = 0x04,
  ENERGY_SAVING = 0x05,
  FAN = 0x06
};

enum class DataParameters : uint8_t {
  AC_POWER = 0x01,
  SET_POINT = 0x02,
  VERTICAL_SWING_MODE = 0x03,
  AC_MODE = 0x04,
  FAN_MODE = 0x05,
  USE_FAHRENHEIT = 0x07,
  DISPLAY_STATUS = 0x09,
  TEN_DEGREE = 0x0A,
  HEALTH_MODE = 0x0B,
  HORIZONTAL_SWING_MODE = 0x0C,
  SELF_CLEANING = 0x0D,
  BEEPER_STATUS = 0x16,
  LOCK_REMOTE = 0x17,
  QUIET_MODE = 0x19,
  FAST_MODE = 0x1A,
  SLEEP_MODE = 0x1B,
};

enum class SpecialMode : uint8_t { NONE = 0x00, ELDERLY = 0x01, CHILDREN = 0x02, PREGNANT = 0x03 };

enum class FanMode : uint8_t { FAN_HIGH = 0x01, FAN_MID = 0x02, FAN_LOW = 0x03, FAN_AUTO = 0x05 };

struct HaierPacketControl {
  // Control bytes starts here
  // 1
  uint8_t set_point;  // Target temperature with 16°C offset (0x00 = 16°C)
  // 2
  uint8_t vertical_swing_mode : 4;  // See enum VerticalSwingMode
  uint8_t : 0;
  // 3
  uint8_t fan_mode : 3;      // See enum FanMode
  uint8_t special_mode : 2;  // See enum SpecialMode
  uint8_t ac_mode : 3;       // See enum ConditioningMode
  // 4
  uint8_t : 8;
  // 5
  uint8_t ten_degree : 1;           // 10 degree status
  uint8_t display_status : 1;       // If 0 disables AC's display
  uint8_t half_degree : 1;          // Use half degree
  uint8_t intelligence_status : 1;  // Intelligence status
  uint8_t pmv_status : 1;           // Comfort/PMV status
  uint8_t use_fahrenheit : 1;       // Use Fahrenheit instead of Celsius
  uint8_t : 1;
  uint8_t steri_clean : 1;
  // 6
  uint8_t ac_power : 1;                 // Is ac on or off
  uint8_t health_mode : 1;              // Health mode (negative ions) on or off
  uint8_t electric_heating_status : 1;  // Electric heating status
  uint8_t fast_mode : 1;                // Fast mode
  uint8_t quiet_mode : 1;               // Quiet mode
  uint8_t sleep_mode : 1;               // Sleep mode
  uint8_t lock_remote : 1;              // Disable remote
  uint8_t beeper_status : 1;  // If 1 disables AC's command feedback beeper (need to be set on every control command)
  // 7
  uint8_t target_humidity;  // Target humidity (0=30% .. 3C=90%, step = 1%)
  // 8
  uint8_t horizontal_swing_mode : 3;  // See enum HorizontalSwingMode
  uint8_t : 3;
  uint8_t human_sensing_status : 2;  // Human sensing status
  // 9
  uint8_t change_filter : 1;  // Filter need replacement
  uint8_t : 0;
  // 10
  uint8_t fresh_air_status : 1;       // Fresh air status
  uint8_t humidification_status : 1;  // Humidification status
  uint8_t pm2p5_cleaning_status : 1;  // PM2.5 cleaning status
  uint8_t ch2o_cleaning_status : 1;   // CH2O cleaning status
  uint8_t self_cleaning_status : 1;   // Self cleaning status
  uint8_t light_status : 1;           // Light status
  uint8_t energy_saving_status : 1;   // Energy saving status
  uint8_t cleaning_time_status : 1;   // Cleaning time (0 - accumulation, 1 - clear)
};

struct HaierPacketSensors {
  // 11
  uint8_t room_temperature;  // 0.5°C step
  // 12
  uint8_t room_humidity;  // 0%-100% with 1% step
  // 13
  uint8_t outdoor_temperature;  // 1°C step, -64°C offset (0=-64°C)
  // 14
  uint8_t pm2p5_level : 2;    // Indoor PM2.5 grade (00: Excellent, 01: good, 02: Medium, 03: Bad)
  uint8_t air_quality : 2;    // Air quality grade (00: Excellent, 01: good, 02: Medium, 03: Bad)
  uint8_t human_sensing : 2;  // Human presence result (00: N/A, 01: not detected, 02: One, 03: Multiple)
  uint8_t : 1;
  uint8_t ac_type : 1;  // 00 - Heat and cool, 01 - Cool only)
  // 15
  uint8_t error_status;  // See enum ErrorStatus
  // 16
  uint8_t operation_source : 2;   // who is controlling AC (00: Other, 01: Remote control, 02: Button, 03: ESP)
  uint8_t operation_mode_hk : 2;  // Homekit only, operation mode (00: Cool, 01: Dry, 02: Heat, 03: Fan)
  uint8_t : 3;
  uint8_t err_confirmation : 1;  // If 1 clear error status
  // 17
  uint16_t total_cleaning_time;  // Cleaning cumulative time (1h step)
  // 19
  uint16_t indoor_pm2p5_value;  // Indoor PM2.5 value (0 ug/m3 -  4095 ug/m3, 1 ug/m3 step)
  // 21
  uint16_t outdoor_pm2p5_value;  // Outdoor PM2.5 value (0 ug/m3 -  4095 ug/m3, 1 ug/m3 step)
  // 23
  uint16_t ch2o_value;  // Formaldehyde value (0 ug/m3 -  10000 ug/m3, 1 ug/m3 step)
  // 25
  uint16_t voc_value;  // VOC value (Volatile Organic Compounds) (0 ug/m3 -  1023 ug/m3, 1 ug/m3 step)
  // 27
  uint16_t co2_value;  // CO2 value (0 PPM -  10000 PPM, 1 PPM step)
};

struct HaierPacketBigData {
  // 29
  uint8_t power[2];  // AC power consumption (0W - 65535W, 1W step)
  // 31
  uint8_t indoor_coil_temperature;  // 0.5°C step, -20°C offset (0=-20°C)
  // 32
  uint8_t outdoor_out_air_temperature;  // 1°C step, -64°C offset (0=-64°C)
  // 33
  uint8_t outdoor_coil_temperature;  // 1°C step, -64°C offset (0=-64°C)
  // 34
  uint8_t outdoor_in_air_temperature;  // 1°C step, -64°C offset (0=-64°C)
  // 35
  uint8_t outdoor_defrost_temperature;  // 1°C step, -64°C offset (0=-64°C)
  // 36
  uint8_t compressor_frequency;  // 1Hz step, 0Hz - 127Hz
  // 37
  uint8_t compressor_current[2];  // 0.1A step, 0.0A - 51.1A (0x0000 - 0x01FF)
  // 39
  uint8_t outdoor_fan_status : 2;  // 0 - off, 1 - on,  2 - information not available
  uint8_t defrost_status : 2;      // 0 - off, 1 - on,  2 - information not available
  uint8_t : 0;
  // 40
  uint8_t compressor_status : 2;               // 0 - off, 1 - on,  2 - information not available
  uint8_t indoor_fan_status : 2;               // 0 - off, 1 - on,  2 - information not available
  uint8_t four_way_valve_status : 2;           // 0 - off, 1 - on,  2 - information not available
  uint8_t indoor_electric_heating_status : 2;  // 0 - off, 1 - on,  2 - information not available
  // 41
  uint8_t expansion_valve_open_degree[2];  // 0 - 4095
};

struct DeviceVersionAnswer {
  char protocol_version[8];
  char software_version[8];
  uint8_t encryption[3];
  char hardware_version[8];
  uint8_t : 8;
  char device_name[8];
  uint8_t functions[2];
};

enum class SubcommandsControl : uint16_t {
  GET_PARAMETERS = 0x4C01,  // Request specific parameters (packet content: parameter ID1 + parameter ID2 + ...)
  GET_USER_DATA = 0x4D01,   // Request all user data from device (packet content: None)
  GET_BIG_DATA = 0x4DFE,    // Request big data information from device (packet content: None)
  SET_PARAMETERS = 0x5C01,  // Set parameters of the device and device return parameters (packet content: parameter ID1
                            // + parameter data1 + parameter ID2 + parameter data 2 + ...)
  SET_SINGLE_PARAMETER = 0x5D00,  // Set single parameter (0x5DXX second byte define parameter ID) and return all user
                                  // data (packet content: ???)
  SET_GROUP_PARAMETERS = 0x6001,  // Set group parameters to device (0x60XX second byte define parameter is group ID,
                                  // the only group mentioned in document is 1) and return all user data (packet
                                  // content: all values like in status packet)
};

const std::string HON_ALARM_MESSAGES[] = {
    "Outdoor module failure",
    "Outdoor defrost sensor failure",
    "Outdoor compressor exhaust sensor failure",
    "Outdoor EEPROM abnormality",
    "Indoor coil sensor failure",
    "Indoor-outdoor communication failure",
    "Power supply overvoltage protection",
    "Communication failure between panel and indoor unit",
    "Outdoor compressor overheat protection",
    "Outdoor environmental sensor abnormality",
    "Full water protection",
    "Indoor EEPROM failure",
    "Outdoor out air sensor failure",
    "CBD and module communication failure",
    "Indoor DC fan failure",
    "Outdoor DC fan failure",
    "Door switch failure",
    "Dust filter needs cleaning reminder",
    "Water shortage protection",
    "Humidity sensor failure",
    "Indoor temperature sensor failure",
    "Manipulator limit failure",
    "Indoor PM2.5 sensor failure",
    "Outdoor PM2.5 sensor failure",
    "Indoor heating overload/high load alarm",
    "Outdoor AC current protection",
    "Outdoor compressor operation abnormality",
    "Outdoor DC current protection",
    "Outdoor no-load failure",
    "CT current abnormality",
    "Indoor cooling freeze protection",
    "High and low pressure protection",
    "Compressor out air temperature is too high",
    "Outdoor evaporator sensor failure",
    "Outdoor cooling overload",
    "Water pump drainage failure",
    "Three-phase power supply failure",
    "Four-way valve failure",
    "External alarm/scraper flow switch failure",
    "Temperature cutoff protection alarm",
    "Different mode operation failure",
    "Electronic expansion valve failure",
    "Dual heat source sensor Tw failure",
    "Communication failure with the wired controller",
    "Indoor unit address duplication failure",
    "50Hz zero crossing failure",
    "Outdoor unit failure",
    "Formaldehyde sensor failure",
    "VOC sensor failure",
    "CO2 sensor failure",
    "Firewall failure",
};

constexpr size_t HON_ALARM_COUNT = sizeof(HON_ALARM_MESSAGES) / sizeof(HON_ALARM_MESSAGES[0]);

}  // namespace hon_protocol
}  // namespace haier
}  // namespace esphome
