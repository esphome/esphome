#include "tas58xx.h"

#include "esphome/core/hal.h"

namespace esphome::tas58xx {

// Remainder of the startup sequence, run after the reset in TAS58xx::init_(). Register 0x00 selects the page.
// Based on the TI PurePath Console (PPC) export for "TAS5825M 2-Band DRC&AGL 2.0 96k".
// Registers 0x46, 0x7D, 0x7E, page 1 register 0x51 and page 2 registers 0x1D, 0x19 are not documented in datasheet.
static const uint8_t STARTUP_SEQUENCE[][2] PROGMEM = {
    {0x03, 0x00},  // DEVICE_CTRL_2: deep sleep, the state the PPC sequence starts from
    {0x7D, 0x11},
    {0x7E, 0xFF},
    {0x00, 0x01},  // Page 1
    {0x51, 0x05},
    {0x00, 0x02},  // Page 2
    {0x1D, 0x00},
    {0x19, 0x80},
    {0x00, 0x00},  // Page 0
    {0x46, 0x11},
    // PPC also writes 0x02 (DEVICE_CTRL_1) and 0x54 (AGAIN) here; init_() writes both from YAML
    {0x53, 0x01},  // ANA_CTRL: in phase, 100kHz bandwidth for high audio performance with Fsw=384kHz
    {0x03, 0x02},  // DEVICE_CTRL_2: Hi-Z

    // Additional register configuration
    // GPIO1 is a dedicated pin on TAS5825M (separate from ADR), used as FAULTZ
    {0x62, 0x0B},  // GPIO1_SEL: GPIO1 as FAULTZ
    {0x60, 0x02},  // GPIO_CTRL: GPIO1 is output
    {0x77, 0x07},  // CBC_CONTROL: enable cycle-by-cycle current limit for warnings and faults
};

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
