#pragma once
#include <cstdint>

namespace esphome {
namespace bl0906 {

// Total power conversion
static const float BL0906_WATT = 16 * 1.097 * 1.097 * (20000 + 20000 + 20000 + 20000 + 20000) /
                                 (40.41259 * ((5.1 + 5.1) * 1000 / 2000) * 1 * 100 * 1 * 1000);
// Total Energy conversion
static const float BL0906_CF = 16 * 4194304 * 0.032768 * 16 /
                               (3600000 * 16 *
                                (40.4125 * ((5.1 + 5.1) * 1000 / 2000) * 1 * 100 * 1 * 1000 /
                                 (1.097 * 1.097 * (20000 + 20000 + 20000 + 20000 + 20000))));
// Frequency conversion
static const float BL0906_FREF = 10000000;
// Temperature conversion
static const float BL0906_TREF = 12.5 / 59 - 40;
// Current conversion
static const float BL0906_IREF = 1.097 / (12875 * 1 * (5.1 + 5.1) * 1000 / 2000);
// Voltage conversion
static const float BL0906_UREF = 1.097 * (20000 + 20000 + 20000 + 20000 + 20000) / (13162 * 1 * 100 * 1000);
// Power conversion
static const float BL0906_PREF = 1.097 * 1.097 * (20000 + 20000 + 20000 + 20000 + 20000) /
                                 (40.41259 * ((5.1 + 5.1) * 1000 / 2000) * 1 * 100 * 1 * 1000);
// Energy conversion
static const float BL0906_EREF = 4194304 * 0.032768 * 16 /
                                 (3600000 * 16 *
                                  (40.4125 * ((5.1 + 5.1) * 1000 / 2000) * 1 * 100 * 1 * 1000 /
                                   (1.097 * 1.097 * (20000 + 20000 + 20000 + 20000 + 20000))));
// Current coefficient
static const float BL0906_KI = 12875 * 1 * (5.1 + 5.1) * 1000 / 2000 / 1.097;
// Power coefficient
static const float BL0906_KP = 40.4125 * ((5.1 + 5.1) * 1000 / 2000) * 1 * 100 * 1 * 1000 / 1.097 / 1.097 /
                               (20000 + 20000 + 20000 + 20000 + 20000);

static const uint8_t USR_WRPROT_WITABLE[6] = {0xCA, 0x9E, 0x55, 0x55, 0x00, 0xB7};
static const uint8_t USR_WRPROT_ONLYREAD[6] = {0xCA, 0x9E, 0x00, 0x00, 0x00, 0x61};

static const uint8_t BL0906_READ_COMMAND = 0x35;
static const uint8_t BL0906_WRITE_COMMAND = 0xCA;

// Register address
// Voltage
static const uint8_t BL0906_V_RMS = 0x16;

// Total power
static const uint8_t BL0906_WATT_SUM = 0x2C;

// Current1~6
static const uint8_t BL0906_I_1_RMS = 0x0D;  // current_1
static const uint8_t BL0906_I_2_RMS = 0x0E;
static const uint8_t BL0906_I_3_RMS = 0x0F;
static const uint8_t BL0906_I_4_RMS = 0x10;
static const uint8_t BL0906_I_5_RMS = 0x13;
static const uint8_t BL0906_I_6_RMS = 0x14;  // current_6

// Power1~6
static const uint8_t BL0906_WATT_1 = 0x23;  // power_1
static const uint8_t BL0906_WATT_2 = 0x24;
static const uint8_t BL0906_WATT_3 = 0x25;
static const uint8_t BL0906_WATT_4 = 0x26;
static const uint8_t BL0906_WATT_5 = 0x29;
static const uint8_t BL0906_WATT_6 = 0x2A;  // power_6

// Active pulse count, unsigned
static const uint8_t BL0906_CF_1_CNT = 0x30;  // Channel_1
static const uint8_t BL0906_CF_2_CNT = 0x31;
static const uint8_t BL0906_CF_3_CNT = 0x32;
static const uint8_t BL0906_CF_4_CNT = 0x33;
static const uint8_t BL0906_CF_5_CNT = 0x36;
static const uint8_t BL0906_CF_6_CNT = 0x37;  // Channel_6

// Total active pulse count, unsigned
static const uint8_t BL0906_CF_SUM_CNT = 0x39;

// Voltage frequency cycle
static const uint8_t BL0906_FREQUENCY = 0x4E;

// Internal temperature
static const uint8_t BL0906_TEMPERATURE = 0x5E;

// Calibration register
// RMS gain adjustment register
static const uint8_t BL0906_RMSGN_1 = 0x6D;  // Channel_1
static const uint8_t BL0906_RMSGN_2 = 0x6E;
static const uint8_t BL0906_RMSGN_3 = 0x6F;
static const uint8_t BL0906_RMSGN_4 = 0x70;
static const uint8_t BL0906_RMSGN_5 = 0x73;
static const uint8_t BL0906_RMSGN_6 = 0x74;  // Channel_6

// RMS offset correction register
static const uint8_t BL0906_RMSOS_1 = 0x78;  // Channel_1
static const uint8_t BL0906_RMSOS_2 = 0x79;
static const uint8_t BL0906_RMSOS_3 = 0x7A;
static const uint8_t BL0906_RMSOS_4 = 0x7B;
static const uint8_t BL0906_RMSOS_5 = 0x7E;
static const uint8_t BL0906_RMSOS_6 = 0x7F;  // Channel_6

// Active power gain adjustment register
static const uint8_t BL0906_WATTGN_1 = 0xB7;  // Channel_1
static const uint8_t BL0906_WATTGN_2 = 0xB8;
static const uint8_t BL0906_WATTGN_3 = 0xB9;
static const uint8_t BL0906_WATTGN_4 = 0xBA;
static const uint8_t BL0906_WATTGN_5 = 0xBD;
static const uint8_t BL0906_WATTGN_6 = 0xBE;  // Channel_6

// User write protection setting register,
// You must first write 0x5555 to the write protection setting register before writing to other registers.
static const uint8_t BL0906_USR_WRPROT = 0x9E;

// Reset Register
static const uint8_t BL0906_SOFT_RESET = 0x9F;

const uint8_t BL0906_INIT[2][6] = {
    // Reset to default
    {BL0906_WRITE_COMMAND, BL0906_SOFT_RESET, 0x5A, 0x5A, 0x5A, 0x52},
    // Enable User Operation Write
    {BL0906_WRITE_COMMAND, BL0906_USR_WRPROT, 0x55, 0x55, 0x00, 0xB7}};

}  // namespace bl0906
}  // namespace esphome
