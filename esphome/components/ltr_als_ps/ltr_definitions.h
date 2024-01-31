#pragma once

#include <cstdint>

namespace esphome {
namespace ltr_als_ps {

enum class CommandRegisters : uint8_t {
  ALS_CONTR = 0x80,         // ALS operation mode control and SW reset
  PS_CONTR = 0x81,          // PS operation mode control
  PS_LED = 0x82,            // PS LED pulse frequency control
  PS_N_PULSES = 0x83,       // PS number of pulses control
  PS_MEAS_RATE = 0x84,      // PS measurement rate in active mode
  MEAS_RATE = 0x85,         // ALS measurement rate in active mode
  PART_ID = 0x86,           // Part Number ID and Revision ID
  MANUFAC_ID = 0x87,        // Manufacturer ID
  ALS_DATA_CH1_0 = 0x88,    // ALS measurement CH1 data, lower byte - infrared only
  ALS_DATA_CH1_1 = 0x89,    // ALS measurement CH1 data, upper byte - infrared only
  ALS_DATA_CH0_0 = 0x8A,    // ALS measurement CH0 data, lower byte - visible + infrared
  ALS_DATA_CH0_1 = 0x8B,    // ALS measurement CH0 data, upper byte - visible + infrared
  ALS_PS_STATUS = 0x8C,     // ALS PS new data status
  PS_DATA_0 = 0x8D,         // PS measurement data, lower byte
  PS_DATA_1 = 0x8E,         // PS measurement data, upper byte
  ALS_PS_INTERRUPT = 0x8F,  // Interrupt status
  PS_THRES_UP_0 = 0x90,     // PS interrupt upper threshold, lower byte
  PS_THRES_UP_1 = 0x91,     // PS interrupt upper threshold, upper byte
  PS_THRES_LOW_0 = 0x92,    // PS interrupt lower threshold, lower byte
  PS_THRES_LOW_1 = 0x93,    // PS interrupt lower threshold, upper byte
  PS_OFFSET_1 = 0x94,       // PS offset, upper byte
  PS_OFFSET_0 = 0x95,       // PS offset, lower byte
                            // 0x96 - reserved
  ALS_THRES_UP_0 = 0x97,    // ALS interrupt upper threshold, lower byte
  ALS_THRES_UP_1 = 0x98,    // ALS interrupt upper threshold, upper byte
  ALS_THRES_LOW_0 = 0x99,   // ALS interrupt lower threshold, lower byte
  ALS_THRES_LOW_1 = 0x9A,   // ALS interrupt lower threshold, upper byte
                            // 0x9B - reserved
                            // 0x9C - reserved
                            // 0x9D - reserved
  INTERRUPT_PERSIST = 0x9E  // Interrupt persistence filter
};

// ALS Sensor gain levels
enum AlsGain : uint8_t {
  GAIN_1 = 0,  // default
  GAIN_2 = 1,
  GAIN_4 = 2,
  GAIN_8 = 3,
  GAIN_48 = 6,
  GAIN_96 = 7,
};
static const uint8_t GAINS_COUNT = 6;

// ALS Sensor integration times
enum IntegrationTime : uint8_t {
  INTEGRATION_TIME_100MS = 0,  // default
  INTEGRATION_TIME_50MS = 1,
  INTEGRATION_TIME_200MS = 2,
  INTEGRATION_TIME_400MS = 3,
  INTEGRATION_TIME_150MS = 4,
  INTEGRATION_TIME_250MS = 5,
  INTEGRATION_TIME_300MS = 6,
  INTEGRATION_TIME_350MS = 7
};
static const uint8_t TIMES_COUNT = 8;

// ALS Sensor measurement repeat rate
enum MeasurementRepeatRate {
  REPEAT_RATE_50MS = 0,
  REPEAT_RATE_100MS = 1,
  REPEAT_RATE_200MS = 2,
  REPEAT_RATE_500MS = 3,  // default
  REPEAT_RATE_1000MS = 4,
  REPEAT_RATE_2000MS = 5
};

// PS Sensor gain levels
enum PsGain : uint8_t {
  PS_GAIN_16 = 0,  // default
  PS_GAIN_32 = 2,
  PS_GAIN_64 = 3,
};

// PS Mode
enum PsMode : uint8_t {
  PS_MODE_STANDBY_00 = 0,  // default
  PS_MODE_STANDBY_01 = 1,
  PS_MODE_ACTIVE_10 = 2,
  PS_MODE_ACTIVE_11 = 3,
};

// LED Pulse Modulation Frequency
enum PsLedFreq : uint8_t {
  PS_LED_FREQ_30KHZ = 0,
  PS_LED_FREQ_40KHZ = 1,
  PS_LED_FREQ_50KHZ = 2,
  PS_LED_FREQ_60KHZ = 3,  // default
  PS_LED_FREQ_70KHZ = 4,
  PS_LED_FREQ_80KHZ = 5,
  PS_LED_FREQ_90KHZ = 6,
  PS_LED_FREQ_100KHZ = 7,
};

// LED current duty
enum PsLedDuty : uint8_t {
  PS_LED_DUTY_25 = 0,
  PS_LED_DUTY_50 = 1,
  PS_LED_DUTY_75 = 2,
  PS_LED_DUTY_100 = 3,  // default
};

// LED pulsed current level
enum PsLedCurrent : uint8_t {
  PS_LED_CURRENT_5MA = 0,
  PS_LED_CURRENT_10MA = 1,
  PS_LED_CURRENT_20MA = 2,
  PS_LED_CURRENT_50MA = 3,
  PS_LED_CURRENT_100MA = 4,  // default
  PS_LED_CURRENT_100MA1 = 5,
  PS_LED_CURRENT_100MA2 = 6,
  PS_LED_CURRENT_100MA3 = 7,
};

// PS measurement rate
enum PsMeasurementRate : uint8_t {
  PS_MEAS_RATE_50MS = 0,
  PS_MEAS_RATE_70MS = 1,
  PS_MEAS_RATE_100MS = 2,
  PS_MEAS_RATE_200MS = 3,
  PS_MEAS_RATE_500MS = 4,  // default
  PS_MEAS_RATE_1000MS = 5,
  PS_MEAS_RATE_2000MS = 6,
  PS_MEAS_RATE_2000MS1 = 7,
  PS_MEAS_RATE_10MS = 8,
};

//
// ALS_CONTR Register (0x80)
//
union AlsControlRegister {
  uint8_t raw;
  struct {
    bool active_mode : 1;
    bool sw_reset : 1;
    AlsGain gain : 3;
    uint8_t reserved : 3;
  } __attribute__((packed));
};

//
// PS_CONTR Register (0x81)
//
union PsControlRegister {
  uint8_t raw;
  struct {
    bool ps_mode_xxx : 1;
    bool ps_mode_active : 1;
    PsGain ps_gain : 2;  // only LTR-659/558
    bool reserved_4 : 1;
    bool ps_saturation_indicator_enable : 1;
    bool reserved_6 : 1;
    bool reserved_7 : 1;
  } __attribute__((packed));
};

//
// PS_LED Register (0x82)
//
union PsLedRegister {
  uint8_t raw;
  struct {
    PsLedCurrent ps_led_current : 3;
    PsLedDuty ps_led_duty : 2;
    PsLedFreq ps_led_freq : 3;
  } __attribute__((packed));
};

//
// PS_N_PULSES Register (0x83)
//
union PsNPulsesRegister {
  uint8_t raw;
  struct {
    uint8_t number_of_pulses : 4;
    uint8_t reserved : 4;
  } __attribute__((packed));
};

//
// PS_MEAS_RATE Register (0x84)
//
union PsMeasurementRateRegister {
  uint8_t raw;
  struct {
    PsMeasurementRate ps_measurement_rate : 4;
    uint8_t reserved : 4;
  } __attribute__((packed));
};

//
// ALS_MEAS_RATE Register (0x85)
//
union MeasurementRateRegister {
  uint8_t raw;
  struct {
    MeasurementRepeatRate measurement_repeat_rate : 3;
    IntegrationTime integration_time : 3;
    bool reserved_6 : 1;
    bool reserved_7 : 1;
  } __attribute__((packed));
};

//
// PART_ID Register (0x86) (Read Only)
//
union PartIdRegister {
  uint8_t raw;
  struct {
    uint8_t part_number_id : 4;
    uint8_t revision_id : 4;
  } __attribute__((packed));
};

//
// ALS_PS_STATUS Register (0x8C) (Read Only)
//
union AlsPsStatusRegister {
  uint8_t raw;
  struct {
    bool ps_new_data : 1;    // 0 - old data, 1 - new data
    bool ps_interrupt : 1;   // 0 - interrupt signal not active, 1 - interrupt signal active
    bool als_new_data : 1;   // 0 - old data, 1 - new data
    bool als_interrupt : 1;  // 0 - interrupt signal not active, 1 - interrupt signal active
    AlsGain gain : 3;        // current ALS gain
    bool data_invalid : 1;
  } __attribute__((packed));
};

//
// PS_DATA_1 Register (0x8E) (Read Only)
//
union PsData1Register {
  uint8_t raw;
  struct {
    uint8_t ps_data_high : 3;
    uint8_t reserved : 4;
    bool ps_saturation_flag : 1;
  } __attribute__((packed));
};

//
// INTERRUPT Register (0x8F) (Read Only)
//
union InterruptRegister {
  uint8_t raw;
  struct {
    bool ps_interrupt : 1;
    bool als_interrupt : 1;
    bool interrupt_polarity : 1;  // 0 - active low (default), 1 - active high
    uint8_t reserved : 5;
  } __attribute__((packed));
};

//
// INTERRUPT_PERSIST Register (0x9E)
//
union InterruptPersistRegister {
  uint8_t raw;
  struct {
    uint8_t als_persist : 4;  // 0 - every ALS cycle, 1 - every 2 ALS cycles, ... 15 - every 16 ALS cycles
    uint8_t ps_persist : 4;   // 0 - every PS cycle, 1 - every 2 PS cycles, ... 15 - every 16 PS cycles
  } __attribute__((packed));
};

}  // namespace ltr_als_ps
}  // namespace esphome
