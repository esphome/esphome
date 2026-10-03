#include "tas58xx.h"

#include "esphome/core/hal.h"

namespace esphome::tas58xx {

// Remainder of the startup sequence, run after the reset. Register 0x00 selects the page.
// All writes except starting and last three register writes are exactly the startup sequence from TI PurePath Console.
// Registers 0x46, 0x7D, 0x7E, page 1 register 0x51 and Page 2 registers 0x1D, 0x19 are not documented in the datasheet.

// clang-format off
static const uint8_t STARTUP_SEQUENCE[][2] PROGMEM = {
    {0x03, 0x00},  // Deep Sleep
    {0x7D, 0x11},
    {0x7E, 0xFF},
    {0x00, 0x01},  // Page 1
    {0x51, 0x05},
    {0x00, 0x02},  // Page 2
    {0x1D, 0x00},
    {0x19, 0x80},
    {0x00, 0x00},  // Page 0
    {0x46, 0x11},
    {0x02, 0x00},  // DEVICE_CTRL_1 - BD MODE, Damp to PBL MODE
    {0x53, 0x01},  // ANA_CTRL PWM Phase Control - in phase
    {0x54, 0x00},  // AGAIN 0dB
    {0x03, 0x02},  // Hi-Z
    {0x61, 0x0B},  // GPIO0_SEL - GPIO0 as FAULTZ output
    {0x60, 0x01},  // GPIO0_OE - Enable GPIO0 as output
    {0x77, 0x07},  // CBC_CONTROL enabling CBC function for warnings and faults
};
// clang-format on

static const LogString *model_name() { return LOG_STR("TAS5825M"); }

// An if chain rather than a switch: a switch table would land in rodata, which is RAM on ESP8266.
static const LogString *fault_name(uint8_t index) {
  if (index == 0)
    return LOG_STR("Right channel over current");
  if (index == 1)
    return LOG_STR("Left channel over current");
  if (index == 2)
    return LOG_STR("Right channel DC fault");
  if (index == 3)
    return LOG_STR("Left channel DC fault");
  if (index == 8)
    return LOG_STR("PVDD under voltage");
  if (index == 9)
    return LOG_STR("PVDD over voltage");
  if (index == 10)
    return LOG_STR("Clock fault");
  if (index == 13)
    return LOG_STR("Load EEPROM error");
  if (index == 14)
    return LOG_STR("BQ write failed");
  if (index == 15)
    return LOG_STR("OTP CRC check error");
  if (index == 16)
    return LOG_STR("Over temperature shutdown");
  if (index == 17)
    return LOG_STR("Left channel CBC over current");
  if (index == 18)
    return LOG_STR("Right channel CBC over current");
  // if (index == 24)
  //   return LOG_STR("Over temperature warning 112C"); // not currently included
  // if (index == 25)
  //   return LOG_STR("Over temperature warning 122C"); // not currently included
  if (index == 26)
    return LOG_STR("Over temperature warning");
  if (index == 27)
    return LOG_STR("Over temperature warning 146C");
  if (index == 28)
    return LOG_STR("Right channel CBC over current warning");
  if (index == 29)
    return LOG_STR("Left channel CBC over current warning");
  return LOG_STR("Unknown fault");
}

const ModelInfo TAS5825M_MODEL = {
    .name = model_name,
    .startup_sequence = STARTUP_SEQUENCE,
    .startup_sequence_length = sizeof(STARTUP_SEQUENCE) / sizeof(STARTUP_SEQUENCE[0]),
    .mixer_book = 0x8C,
    .mixer_page = 0x0B,
    .mixer_register = 0x14,
    // The clock fault is left out of the log and have_fault: it is set whenever the I2S clock stops, which is normal
    .fault_error_mask = 0x0007E30F,
    .fault_warning_mask =
        0x3C000000,  // 0x3F000000 if OVER_TEMP_122C_WARNING and OVER_TEMP_112C_WARNING included in future
    // DC and over current faults keep the output off until cleared (datasheet 7.5.3.3.1, 7.5.3.3.2). They are not
    // cleared automatically: a DC fault re-trips only after 570 ms, so a clear on every poll would pass DC to the
    // speaker.
    .fault_output_off_mask = 0x0000000F,
    .fault_latched_mask = 0x3F07E70F,
    .fault_name = fault_name,
    .fault_sensor_bits =
        {
            3,   // FAULT_SENSOR_LEFT_CHANNEL_DC_FAULT
            2,   // FAULT_SENSOR_RIGHT_CHANNEL_DC_FAULT
            1,   // FAULT_SENSOR_LEFT_CHANNEL_OVER_CURRENT
            0,   // FAULT_SENSOR_RIGHT_CHANNEL_OVER_CURRENT
            15,  // FAULT_SENSOR_OTP_CRC_CHECK
            14,  // FAULT_SENSOR_BQ_WRITE_FAILED
            10,  // FAULT_SENSOR_CLOCK_FAULT
            9,   // FAULT_SENSOR_PVDD_OVER_VOLTAGE
            8,   // FAULT_SENSOR_PVDD_UNDER_VOLTAGE
            16,  // FAULT_SENSOR_OVER_TEMP_SHUTDOWN
            26,  // FAULT_SENSOR_OVER_TEMP_WARNING
            13,  // FAULT_SENSOR_LOAD_EEPROM_ERROR
            18,  // FAULT_SENSOR_RIGHT_CHANNEL_CBC_OVER_CURRENT
            17,  // FAULT_SENSOR_LEFT_CHANNEL_CBC_OVER_CURRENT
            29,  // FAULT_SENSOR_LEFT_CHANNEL_CBC_OVER_CURRENT_WARNING
            28,  // FAULT_SENSOR_RIGHT_CHANNEL_CBC_OVER_CURRENT_WARNING
            27,  // FAULT_SENSOR_OVER_TEMP_146C_WARNING
                 // 25,  // FAULT_SENSOR_OVER_TEMP_122C_WARNING // not currently included
                 // 24,  // FAULT_SENSOR_OVER_TEMP_112C_WARNING // not currently included
        },
};

}  // namespace esphome::tas58xx
