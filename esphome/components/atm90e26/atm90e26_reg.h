#pragma once

namespace esphome {
namespace atm90e26 {

/* Status and Special Register */
static const uint8_t ATM90E26_REGISTER_SOFTRESET = 0x00;  // Software Reset
static const uint8_t ATM90E26_REGISTER_SYSSTATUS = 0x01;  // System Status
static const uint8_t ATM90E26_REGISTER_FUNCEN = 0x02;     // Function Enable
static const uint8_t ATM90E26_REGISTER_SAGTH = 0x03;      // Voltage Sag Threshold
static const uint8_t ATM90E26_REGISTER_SMALLPMOD = 0x04;  // Small-Power Mode
static const uint8_t ATM90E26_REGISTER_LASTDATA = 0x06;   // Last Read/Write SPI/UART Value

/* Metering Calibration and Configuration Register */
static const uint8_t ATM90E26_REGISTER_LSB = 0x08;       // RMS/Power 16-bit LSB
static const uint8_t ATM90E26_REGISTER_CALSTART = 0x20;  // Calibration Start Command
static const uint8_t ATM90E26_REGISTER_PLCONSTH = 0x21;  // High Word of PL_Constant
static const uint8_t ATM90E26_REGISTER_PLCONSTL = 0x22;  // Low Word of PL_Constant
static const uint8_t ATM90E26_REGISTER_LGAIN = 0x23;     // L Line Calibration Gain
static const uint8_t ATM90E26_REGISTER_LPHI = 0x24;      // L Line Calibration Angle
static const uint8_t ATM90E26_REGISTER_NGAIN = 0x25;     // N Line Calibration Gain
static const uint8_t ATM90E26_REGISTER_NPHI = 0x26;      // N Line Calibration Angle
static const uint8_t ATM90E26_REGISTER_PSTARTTH = 0x27;  // Active Startup Power Threshold
static const uint8_t ATM90E26_REGISTER_PNOLTH = 0x28;    // Active No-Load Power Threshold
static const uint8_t ATM90E26_REGISTER_QSTARTTH = 0x29;  // Reactive Startup Power Threshold
static const uint8_t ATM90E26_REGISTER_QNOLTH = 0x2A;    // Reactive No-Load Power Threshold
static const uint8_t ATM90E26_REGISTER_MMODE = 0x2B;     // Metering Mode Configuration
static const uint8_t ATM90E26_REGISTER_CS1 = 0x2C;       // Checksum 1

/* Measurement Calibration Register */
static const uint8_t ATM90E26_REGISTER_ADJSTART = 0x30;  // Measurement Calibration Start Command
static const uint8_t ATM90E26_REGISTER_UGAIN = 0x31;     // Voltage RMS Gain
static const uint8_t ATM90E26_REGISTER_IGAINL = 0x32;    // L Line Current RMS Gain
static const uint8_t ATM90E26_REGISTER_IGAINN = 0x33;    // N Line Current RMS Gain
static const uint8_t ATM90E26_REGISTER_UOFFSET = 0x34;   // Voltage Offset
static const uint8_t ATM90E26_REGISTER_IOFFSETL = 0x35;  // L Line Current Offset
static const uint8_t ATM90E26_REGISTER_IOFFSETN = 0x36;  // N Line Current Offse
static const uint8_t ATM90E26_REGISTER_POFFSETL = 0x37;  // L Line Active Power Offset
static const uint8_t ATM90E26_REGISTER_QOFFSETL = 0x38;  // L Line Reactive Power Offset
static const uint8_t ATM90E26_REGISTER_POFFSETN = 0x39;  // N Line Active Power Offset
static const uint8_t ATM90E26_REGISTER_QOFFSETN = 0x3A;  // N Line Reactive Power Offset
static const uint8_t ATM90E26_REGISTER_CS2 = 0x3B;       // Checksum 2

/* Energy Register */
static const uint8_t ATM90E26_REGISTER_APENERGY = 0x40;  // Forward Active Energy
static const uint8_t ATM90E26_REGISTER_ANENERGY = 0x41;  // Reverse Active Energy
static const uint8_t ATM90E26_REGISTER_ATENERGY = 0x42;  // Absolute Active Energy
static const uint8_t ATM90E26_REGISTER_RPENERGY = 0x43;  // Forward (Inductive) Reactive Energy
static const uint8_t ATM90E26_REGISTER_RNENERG = 0x44;   // Reverse (Capacitive) Reactive Energy
static const uint8_t ATM90E26_REGISTER_RTENERGY = 0x45;  // Absolute Reactive Energy
static const uint8_t ATM90E26_REGISTER_ENSTATUS = 0x46;  // Metering Status

/* Measurement Register */
static const uint8_t ATM90E26_REGISTER_IRMS = 0x48;     // L Line Current RMS
static const uint8_t ATM90E26_REGISTER_URMS = 0x49;     // Voltage RMS
static const uint8_t ATM90E26_REGISTER_PMEAN = 0x4A;    // L Line Mean Active Power
static const uint8_t ATM90E26_REGISTER_QMEAN = 0x4B;    // L Line Mean Reactive Power
static const uint8_t ATM90E26_REGISTER_FREQ = 0x4C;     // Voltage Frequency
static const uint8_t ATM90E26_REGISTER_POWERF = 0x4D;   // L Line Power Factor
static const uint8_t ATM90E26_REGISTER_PANGLE = 0x4E;   // Phase Angle between Voltage and L Line Current
static const uint8_t ATM90E26_REGISTER_SMEAN = 0x4F;    // L Line Mean Apparent Power
static const uint8_t ATM90E26_REGISTER_IRMS2 = 0x68;    // N Line Current rms
static const uint8_t ATM90E26_REGISTER_PMEAN2 = 0x6A;   // N Line Mean Active Power
static const uint8_t ATM90E26_REGISTER_QMEAN2 = 0x6B;   // N Line Mean Reactive Power
static const uint8_t ATM90E26_REGISTER_POWERF2 = 0x6D;  // N Line Power Factor
static const uint8_t ATM90E26_REGISTER_PANGLE2 = 0x6E;  // Phase Angle between Voltage and N Line Current
static const uint8_t ATM90E26_REGISTER_SMEAN2 = 0x6F;   // N Line Mean Apparent Power

}  // namespace atm90e26
}  // namespace esphome
