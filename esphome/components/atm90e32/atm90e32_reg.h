#pragma once

namespace esphome {
namespace atm90e32 {

/* STATUS REGISTERS */
static const uint16_t ATM90E32_REGISTER_METEREN = 0x00;        // Metering Enable
static const uint16_t ATM90E32_REGISTER_CHANNELMAPI = 0x01;    // Current Channel Mapping Configuration
static const uint16_t ATM90E32_REGISTER_CHANNELMAPU = 0x02;    // Voltage Channel Mapping Configuration
static const uint16_t ATM90E32_REGISTER_SAGPEAKDETCFG = 0x05;  // Sag and Peak Detector Period Configuration
static const uint16_t ATM90E32_REGISTER_OVTH = 0x06;           // Over Voltage Threshold
static const uint16_t ATM90E32_REGISTER_ZXCONFIG = 0x07;       // Zero-Crossing Config
static const uint16_t ATM90E32_REGISTER_SAGTH = 0x08;          // Voltage Sag Th
static const uint16_t ATM90E32_REGISTER_PHASELOSSTH = 0x09;    // Voltage Phase Losing Th
static const uint16_t ATM90E32_REGISTER_INWARNTH = 0x0A;       // Neutral Current (Calculated) Warning Threshold
static const uint16_t ATM90E32_REGISTER_OITH = 0x0B;           // Over Current Threshold
static const uint16_t ATM90E32_REGISTER_FREQLOTH = 0x0C;       // Low Threshold for Frequency Detection
static const uint16_t ATM90E32_REGISTER_FREQHITH = 0x0D;       // High Threshold for Frequency Detection
static const uint16_t ATM90E32_REGISTER_PMPWRCTRL = 0x0E;      // Partial Measurement Mode Power Control
static const uint16_t ATM90E32_REGISTER_IRQ0MERGECFG = 0x0F;   // IRQ0 Merge Configuration

/* EMM STATUS REGISTERS */
static const uint16_t ATM90E32_REGISTER_SOFTRESET = 0x70;       // Software Reset
static const uint16_t ATM90E32_REGISTER_EMMSTATE0 = 0x71;       // EMM State 0
static const uint16_t ATM90E32_REGISTER_EMMSTATE1 = 0x72;       // EMM State 1
static const uint16_t ATM90E32_REGISTER_EMMINTSTATE0 = 0x73;    // EMM Interrupt Status 0
static const uint16_t ATM90E32_REGISTER_EMMINTSTATE1 = 0x74;    // EMM Interrupt Status 1
static const uint16_t ATM90E32_REGISTER_EMMINTEN0 = 0x75;       // EMM Interrupt Enable 0
static const uint16_t ATM90E32_REGISTER_EMMINTEN1 = 0x76;       // EMM Interrupt Enable 1
static const uint16_t ATM90E32_REGISTER_LASTSPIDATA = 0x78;     // Last Read/Write SPI Value
static const uint16_t ATM90E32_REGISTER_CRCERRSTATUS = 0x79;    // CRC Error Status
static const uint16_t ATM90E32_REGISTER_CRCDIGEST = 0x7A;       // CRC Digest
static const uint16_t ATM90E32_REGISTER_CFGREGACCEN = 0x7F;     // Configure Register Access Enable
static const uint16_t ATM90E32_STATUS_S0_OIPHASEAST = 1 << 15;  // Over current on phase A
static const uint16_t ATM90E32_STATUS_S0_OIPHASEBST = 1 << 14;  // Over current on phase B
static const uint16_t ATM90E32_STATUS_S0_OIPHASECST = 1 << 13;  // Over current on phase C
static const uint16_t ATM90E32_STATUS_S0_OVPHASEAST = 1 << 12;  // Over voltage on phase A
static const uint16_t ATM90E32_STATUS_S0_OVPHASEBST = 1 << 11;  // Over voltage on phase B
static const uint16_t ATM90E32_STATUS_S0_OVPHASECST = 1 << 10;  // Over voltage on phase C
static const uint16_t ATM90E32_STATUS_S0_UREVWNST = 1 << 9;     // Voltage Phase Sequence Error status
static const uint16_t ATM90E32_STATUS_S0_IREVWNST = 1 << 8;     // Current Phase Sequence Error status
static const uint16_t ATM90E32_STATUS_S0_INOV0ST = 1 << 7;      // Calculated N line current greater than INWarnTh reg
static const uint16_t ATM90E32_STATUS_S0_TQNOLOADST = 1 << 6;   // All phase sum reactive power no-load condition status
static const uint16_t ATM90E32_STATUS_S0_TPNOLOADST = 1 << 5;   // All phase sum active power no-load condition status
static const uint16_t ATM90E32_STATUS_S0_TASNOLOADST = 1 << 4;  // All phase sum apparent power no-load status
static const uint16_t ATM90E32_STATUS_S0_CF1REVST = 1 << 3;     // Energy for CF1 Forward/Reverse status
static const uint16_t ATM90E32_STATUS_S0_CF2REVST = 1 << 2;     // Energy for CF2 Forward/Reverse status
static const uint16_t ATM90E32_STATUS_S0_CF3REVST = 1 << 1;     // Energy for CF3 Forward/Reverse status
static const uint16_t ATM90E32_STATUS_S0_CF4REVST = 1 << 0;     // Energy for CF4 Forward/Reverse status
static const uint16_t ATM90E32_STATUS_S1_FREQHIST = 1 << 15;    // Frequency is greater than the high threshold
static const uint16_t ATM90E32_STATUS_S1_SAGPHASEAST = 1 << 14;   // Voltage sag on phase A
static const uint16_t ATM90E32_STATUS_S1_SAGPHASEBST = 1 << 13;   // Voltage sag on phase B
static const uint16_t ATM90E32_STATUS_S1_SAGPHASECST = 1 << 12;   // Voltage sag on phase C
static const uint16_t ATM90E32_STATUS_S1_FREQLOST = 1 << 11;      // Frequency is lesser than the low threshold
static const uint16_t ATM90E32_STATUS_S1_PHASELOSSAST = 1 << 10;  // Phase loss in Phase A
static const uint16_t ATM90E32_STATUS_S1_PHASELOSSBST = 1 << 9;   // Phase loss in Phase B
static const uint16_t ATM90E32_STATUS_S1_PHASELOSSCST = 1 << 8;   // Phase loss in Phase C
static const uint16_t ATM90E32_STATUS_S1_QEREGTPST = 1 << 7;      // ReActive Energy register of sum (T) Positive Status
static const uint16_t ATM90E32_STATUS_S1_QEREGAPST = 1 << 6;  // ReActive Energy register of Channel A Positive Status
static const uint16_t ATM90E32_STATUS_S1_QEREGBPST = 1 << 5;  // ReActive Energy register of Channel B Positive Status
static const uint16_t ATM90E32_STATUS_S1_QEREGCPST = 1 << 4;  // ReActive Energy register of Channel C Positive Status
static const uint16_t ATM90E32_STATUS_S1_PEREGTPST = 1 << 3;  // Active Energy register of sum (T) Positive Status
static const uint16_t ATM90E32_STATUS_S1_PEREGAPST = 1 << 2;  // Active Energy register of Channel A Positive Status
static const uint16_t ATM90E32_STATUS_S1_PEREGBPST = 1 << 1;  // Active Energy register of Channel B Positive Status
static const uint16_t ATM90E32_STATUS_S1_PEREGCPST = 1 << 0;  // Active Energy register of Channel C Positive Status

/* LOW POWER MODE REGISTERS - NOT USED */
static const uint16_t ATM90E32_REGISTER_DETECTCTRL = 0x10;
static const uint16_t ATM90E32_REGISTER_DETECTTH1 = 0x11;
static const uint16_t ATM90E32_REGISTER_DETECTTH2 = 0x12;
static const uint16_t ATM90E32_REGISTER_DETECTTH3 = 0x13;
static const uint16_t ATM90E32_REGISTER_PMOFFSETA = 0x14;
static const uint16_t ATM90E32_REGISTER_PMOFFSETB = 0x15;
static const uint16_t ATM90E32_REGISTER_PMOFFSETC = 0x16;
static const uint16_t ATM90E32_REGISTER_PMPGA = 0x17;
static const uint16_t ATM90E32_REGISTER_PMIRMSA = 0x18;
static const uint16_t ATM90E32_REGISTER_PMIRMSB = 0x19;
static const uint16_t ATM90E32_REGISTER_PMIRMSC = 0x1A;
static const uint16_t ATM90E32_REGISTER_PMCONFIG = 0x10B;
static const uint16_t ATM90E32_REGISTER_PMAVGSAMPLES = 0x1C;
static const uint16_t ATM90E32_REGISTER_PMIRMSLSB = 0x1D;

/* CONFIGURATION REGISTERS */
static const uint16_t ATM90E32_REGISTER_PLCONSTH = 0x31;  // High Word of PL_Constant
static const uint16_t ATM90E32_REGISTER_PLCONSTL = 0x32;  // Low Word of PL_Constant
static const uint16_t ATM90E32_REGISTER_MMODE0 = 0x33;    // Metering Mode Config
static const uint16_t ATM90E32_REGISTER_MMODE1 = 0x34;    // PGA Gain Configuration for Current Channels
static const uint16_t ATM90E32_REGISTER_PSTARTTH = 0x35;  // Startup Power Th (P)
static const uint16_t ATM90E32_REGISTER_QSTARTTH = 0x36;  // Startup Power Th (Q)
static const uint16_t ATM90E32_REGISTER_SSTARTTH = 0x37;  // Startup Power Th (S)
static const uint16_t ATM90E32_REGISTER_PPHASETH = 0x38;  // Startup Power Accum Th (P)
static const uint16_t ATM90E32_REGISTER_QPHASETH = 0x39;  // Startup Power Accum Th (Q)
static const uint16_t ATM90E32_REGISTER_SPHASETH = 0x3A;  // Startup Power Accum Th (S)

/* CALIBRATION REGISTERS */
static const uint16_t ATM90E32_REGISTER_POFFSETA = 0x41;  // A Line Power Offset (P)
static const uint16_t ATM90E32_REGISTER_QOFFSETA = 0x42;  // A Line Power Offset (Q)
static const uint16_t ATM90E32_REGISTER_POFFSETB = 0x43;  // B Line Power Offset (P)
static const uint16_t ATM90E32_REGISTER_QOFFSETB = 0x44;  // B Line Power Offset (Q)
static const uint16_t ATM90E32_REGISTER_POFFSETC = 0x45;  // C Line Power Offset (P)
static const uint16_t ATM90E32_REGISTER_QOFFSETC = 0x46;  // C Line Power Offset (Q)
static const uint16_t ATM90E32_REGISTER_PQGAINA = 0x47;   // A Line Calibration Gain
static const uint16_t ATM90E32_REGISTER_PHIA = 0x48;      // A Line Calibration Angle
static const uint16_t ATM90E32_REGISTER_PQGAINB = 0x49;   // B Line Calibration Gain
static const uint16_t ATM90E32_REGISTER_PHIB = 0x4A;      // B Line Calibration Angle
static const uint16_t ATM90E32_REGISTER_PQGAINC = 0x4B;   // C Line Calibration Gain
static const uint16_t ATM90E32_REGISTER_PHIC = 0x4C;      // C Line Calibration Angle

/* FUNDAMENTAL/HARMONIC ENERGY CALIBRATION REGISTERS */
static const uint16_t ATM90E32_REGISTER_POFFSETAF = 0x51;  // A Fund Power Offset (P)
static const uint16_t ATM90E32_REGISTER_POFFSETBF = 0x52;  // B Fund Power Offset (P)
static const uint16_t ATM90E32_REGISTER_POFFSETCF = 0x53;  // C Fund Power Offset (P)
static const uint16_t ATM90E32_REGISTER_PGAINAF = 0x54;    // A Fund Power Gain (P)
static const uint16_t ATM90E32_REGISTER_PGAINBF = 0x55;    // B Fund Power Gain (P)
static const uint16_t ATM90E32_REGISTER_PGAINCF = 0x56;    // C Fund Power Gain (P)

/* MEASUREMENT CALIBRATION REGISTERS */
static const uint16_t ATM90E32_REGISTER_UGAINA = 0x61;    // A Voltage RMS Gain
static const uint16_t ATM90E32_REGISTER_IGAINA = 0x62;    // A Current RMS Gain
static const uint16_t ATM90E32_REGISTER_UOFFSETA = 0x63;  // A Voltage Offset
static const uint16_t ATM90E32_REGISTER_IOFFSETA = 0x64;  // A Current Offset
static const uint16_t ATM90E32_REGISTER_UGAINB = 0x65;    // B Voltage RMS Gain
static const uint16_t ATM90E32_REGISTER_IGAINB = 0x66;    // B Current RMS Gain
static const uint16_t ATM90E32_REGISTER_UOFFSETB = 0x67;  // B Voltage Offset
static const uint16_t ATM90E32_REGISTER_IOFFSETB = 0x68;  // B Current Offset
static const uint16_t ATM90E32_REGISTER_UGAINC = 0x69;    // C Voltage RMS Gain
static const uint16_t ATM90E32_REGISTER_IGAINC = 0x6A;    // C Current RMS Gain
static const uint16_t ATM90E32_REGISTER_UOFFSETC = 0x6B;  // C Voltage Offset
static const uint16_t ATM90E32_REGISTER_IOFFSETC = 0x6C;  // C Current Offset
static const uint16_t ATM90E32_REGISTER_IOFFSETN = 0x6E;  // N Current Offset

/* ENERGY REGISTERS */
static const uint16_t ATM90E32_REGISTER_APENERGYT = 0x80;  // Total Forward Active
static const uint16_t ATM90E32_REGISTER_APENERGYA = 0x81;  // A Forward Active
static const uint16_t ATM90E32_REGISTER_APENERGYB = 0x82;  // B Forward Active
static const uint16_t ATM90E32_REGISTER_APENERGYC = 0x83;  // C Forward Active
static const uint16_t ATM90E32_REGISTER_ANENERGYT = 0x84;  // Total Reverse Active
static const uint16_t ATM90E32_REGISTER_ANENERGYA = 0x85;  // A Reverse Active
static const uint16_t ATM90E32_REGISTER_ANENERGYB = 0x86;  // B Reverse Active
static const uint16_t ATM90E32_REGISTER_ANENERGYC = 0x87;  // C Reverse Active
static const uint16_t ATM90E32_REGISTER_RPENERGYT = 0x88;  // Total Forward Reactive
static const uint16_t ATM90E32_REGISTER_RPENERGYA = 0x89;  // A Forward Reactive
static const uint16_t ATM90E32_REGISTER_RPENERGYB = 0x8A;  // B Forward Reactive
static const uint16_t ATM90E32_REGISTER_RPENERGYC = 0x8B;  // C Forward Reactive
static const uint16_t ATM90E32_REGISTER_RNENERGYT = 0x8C;  // Total Reverse Reactive
static const uint16_t ATM90E32_REGISTER_RNENERGYA = 0x8D;  // A Reverse Reactive
static const uint16_t ATM90E32_REGISTER_RNENERGYB = 0x8E;  // B Reverse Reactive
static const uint16_t ATM90E32_REGISTER_RNENERGYC = 0x8F;  // C Reverse Reactive

static const uint16_t ATM90E32_REGISTER_SAENERGYT = 0x90;  // Total Apparent Energy
static const uint16_t ATM90E32_REGISTER_SENERGYA = 0x91;   // A Apparent Energy
static const uint16_t ATM90E32_REGISTER_SENERGYB = 0x92;   // B Apparent Energy
static const uint16_t ATM90E32_REGISTER_SENERGYC = 0x93;   // C Apparent Energy

/* FUNDAMENTAL / HARMONIC ENERGY REGISTERS */
static const uint16_t ATM90E32_REGISTER_APENERGYTF = 0xA0;  // Total Forward Fund. Energy
static const uint16_t ATM90E32_REGISTER_APENERGYAF = 0xA1;  // A Forward Fund. Energy
static const uint16_t ATM90E32_REGISTER_APENERGYBF = 0xA2;  // B Forward Fund. Energy
static const uint16_t ATM90E32_REGISTER_APENERGYCF = 0xA3;  // C Forward Fund. Energy
static const uint16_t ATM90E32_REGISTER_ANENERGYTF = 0xA4;  // Total Reverse Fund Energy
static const uint16_t ATM90E32_REGISTER_ANENERGYAF = 0xA5;  // A Reverse Fund. Energy
static const uint16_t ATM90E32_REGISTER_ANENERGYBF = 0xA6;  // B Reverse Fund. Energy
static const uint16_t ATM90E32_REGISTER_ANENERGYCF = 0xA7;  // C Reverse Fund. Energy
static const uint16_t ATM90E32_REGISTER_APENERGYTH = 0xA8;  // Total Forward Harm. Energy
static const uint16_t ATM90E32_REGISTER_APENERGYAH = 0xA9;  // A Forward Harm. Energy
static const uint16_t ATM90E32_REGISTER_APENERGYBH = 0xAA;  // B Forward Harm. Energy
static const uint16_t ATM90E32_REGISTER_APENERGYCH = 0xAB;  // C Forward Harm. Energy
static const uint16_t ATM90E32_REGISTER_ANENERGYTH = 0xAC;  // Total Reverse Harm. Energy
static const uint16_t ATM90E32_REGISTER_ANENERGYAH = 0xAD;  // A Reverse Harm. Energy
static const uint16_t ATM90E32_REGISTER_ANENERGYBH = 0xAE;  // B Reverse Harm. Energy
static const uint16_t ATM90E32_REGISTER_ANENERGYCH = 0xAF;  // C Reverse Harm. Energy

/* POWER & P.F. REGISTERS */
static const uint16_t ATM90E32_REGISTER_PMEANT = 0xB0;   // Total Mean Power (P)
static const uint16_t ATM90E32_REGISTER_PMEANA = 0xB1;   // A Mean Power (P)
static const uint16_t ATM90E32_REGISTER_PMEANB = 0xB2;   // B Mean Power (P)
static const uint16_t ATM90E32_REGISTER_PMEANC = 0xB3;   // C Mean Power (P)
static const uint16_t ATM90E32_REGISTER_QMEANT = 0xB4;   // Total Mean Power (Q)
static const uint16_t ATM90E32_REGISTER_QMEANA = 0xB5;   // A Mean Power (Q)
static const uint16_t ATM90E32_REGISTER_QMEANB = 0xB6;   // B Mean Power (Q)
static const uint16_t ATM90E32_REGISTER_QMEANC = 0xB7;   // C Mean Power (Q)
static const uint16_t ATM90E32_REGISTER_SMEANT = 0xB8;   // Total Mean Power (S)
static const uint16_t ATM90E32_REGISTER_SMEANA = 0xB9;   // A Mean Power (S)
static const uint16_t ATM90E32_REGISTER_SMEANB = 0xBA;   // B Mean Power (S)
static const uint16_t ATM90E32_REGISTER_SMEANC = 0xBB;   // C Mean Power (S)
static const uint16_t ATM90E32_REGISTER_PFMEANT = 0xBC;  // Mean Power Factor
static const uint16_t ATM90E32_REGISTER_PFMEANA = 0xBD;  // A Power Factor
static const uint16_t ATM90E32_REGISTER_PFMEANB = 0xBE;  // B Power Factor
static const uint16_t ATM90E32_REGISTER_PFMEANC = 0xBF;  // C Power Factor

static const uint16_t ATM90E32_REGISTER_PMEANTLSB = 0xC0;   // Lower Word (Tot. Act. Power)
static const uint16_t ATM90E32_REGISTER_PMEANALSB = 0xC1;   // Lower Word (A Act. Power)
static const uint16_t ATM90E32_REGISTER_PMEANBLSB = 0xC2;   // Lower Word (B Act. Power)
static const uint16_t ATM90E32_REGISTER_PMEANCLSB = 0xC3;   // Lower Word (C Act. Power)
static const uint16_t ATM90E32_REGISTER_QMEANTLSB = 0xC4;   // Lower Word (Tot. React. Power)
static const uint16_t ATM90E32_REGISTER_QMEANALSB = 0xC5;   // Lower Word (A React. Power)
static const uint16_t ATM90E32_REGISTER_QMEANBLSB = 0xC6;   // Lower Word (B React. Power)
static const uint16_t ATM90E32_REGISTER_QMEANCLSB = 0xC7;   // Lower Word (C React. Power)
static const uint16_t ATM90E32_REGISTER_SAMEANTLSB = 0xC8;  // Lower Word (Tot. App. Power)
static const uint16_t ATM90E32_REGISTER_SMEANALSB = 0xC9;   // Lower Word (A App. Power)
static const uint16_t ATM90E32_REGISTER_SMEANBLSB = 0xCA;   // Lower Word (B App. Power)
static const uint16_t ATM90E32_REGISTER_SMEANCLSB = 0xCB;   // Lower Word (C App. Power)

/* FUND/HARM POWER & V/I RMS REGISTERS */
static const uint16_t ATM90E32_REGISTER_PMEANTF = 0xD0;  // Total Active Fund. Power
static const uint16_t ATM90E32_REGISTER_PMEANAF = 0xD1;  // A Active Fund. Power
static const uint16_t ATM90E32_REGISTER_PMEANBF = 0xD2;  // B Active Fund. Power
static const uint16_t ATM90E32_REGISTER_PMEANCF = 0xD3;  // C Active Fund. Power
static const uint16_t ATM90E32_REGISTER_PMEANTH = 0xD4;  // Total Active Harm. Power
static const uint16_t ATM90E32_REGISTER_PMEANAH = 0xD5;  // A Active Harm. Power
static const uint16_t ATM90E32_REGISTER_PMEANBH = 0xD6;  // B Active Harm. Power
static const uint16_t ATM90E32_REGISTER_PMEANCH = 0xD7;  // C Active Harm. Power
static const uint16_t ATM90E32_REGISTER_URMSA = 0xD9;    // A RMS Voltage
static const uint16_t ATM90E32_REGISTER_URMSB = 0xDA;    // B RMS Voltage
static const uint16_t ATM90E32_REGISTER_URMSC = 0xDB;    // C RMS Voltage
static const uint16_t ATM90E32_REGISTER_IRMSA = 0xDD;    // A RMS Current
static const uint16_t ATM90E32_REGISTER_IRMSB = 0xDE;    // B RMS Current
static const uint16_t ATM90E32_REGISTER_IRMSC = 0xDF;    // C RMS Current
static const uint16_t ATM90E32_REGISTER_IRMSN = 0xD8;    // Calculated N RMS Current

static const uint16_t ATM90E32_REGISTER_PMEANTFLSB = 0xE0;  // Lower Word (Tot. Act. Fund. Power)
static const uint16_t ATM90E32_REGISTER_PMEANAFLSB = 0xE1;  // Lower Word (A Act. Fund. Power)
static const uint16_t ATM90E32_REGISTER_PMEANBFLSB = 0xE2;  // Lower Word (B Act. Fund. Power)
static const uint16_t ATM90E32_REGISTER_PMEANCFLSB = 0xE3;  // Lower Word (C Act. Fund. Power)
static const uint16_t ATM90E32_REGISTER_PMEANTHLSB = 0xE4;  // Lower Word (Tot. Act. Harm. Power)
static const uint16_t ATM90E32_REGISTER_PMEANAHLSB = 0xE5;  // Lower Word (A Act. Harm. Power)
static const uint16_t ATM90E32_REGISTER_PMEANBHLSB = 0xE6;  // Lower Word (B Act. Harm. Power)
static const uint16_t ATM90E32_REGISTER_PMEANCHLSB = 0xE7;  // Lower Word (C Act. Harm. Power)
static const uint16_t ATM90E32_REGISTER_URMSALSB = 0xE9;    // Lower Word (A RMS Voltage)
static const uint16_t ATM90E32_REGISTER_URMSBLSB = 0xEA;    // Lower Word (B RMS Voltage)
static const uint16_t ATM90E32_REGISTER_URMSCLSB = 0xEB;    // Lower Word (C RMS Voltage)
static const uint16_t ATM90E32_REGISTER_IRMSALSB = 0xED;    // Lower Word (A RMS Current)
static const uint16_t ATM90E32_REGISTER_IRMSBLSB = 0xEE;    // Lower Word (B RMS Current)
static const uint16_t ATM90E32_REGISTER_IRMSCLSB = 0xEF;    // Lower Word (C RMS Current)

/* THD, FREQUENCY, ANGLE & TEMPTEMP REGISTERS*/
static const uint16_t ATM90E32_REGISTER_UPEAKA = 0xF1;   // A Voltage Peak
static const uint16_t ATM90E32_REGISTER_UPEAKB = 0xF2;   // B Voltage Peak
static const uint16_t ATM90E32_REGISTER_UPEAKC = 0xF3;   // C Voltage Peak
static const uint16_t ATM90E32_REGISTER_IPEAKA = 0xF5;   // A Current Peak
static const uint16_t ATM90E32_REGISTER_IPEAKB = 0xF6;   // B Current Peak
static const uint16_t ATM90E32_REGISTER_IPEAKC = 0xF7;   // C Current Peak
static const uint16_t ATM90E32_REGISTER_FREQ = 0xF8;     // Frequency
static const uint16_t ATM90E32_REGISTER_PANGLEA = 0xF9;  // A Mean Phase Angle
static const uint16_t ATM90E32_REGISTER_PANGLEB = 0xFA;  // B Mean Phase Angle
static const uint16_t ATM90E32_REGISTER_PANGLEC = 0xFB;  // C Mean Phase Angle
static const uint16_t ATM90E32_REGISTER_TEMP = 0xFC;     // Measured Temperature
static const uint16_t ATM90E32_REGISTER_UANGLEA = 0xFD;  // A Voltage Phase Angle
static const uint16_t ATM90E32_REGISTER_UANGLEB = 0xFE;  // B Voltage Phase Angle
static const uint16_t ATM90E32_REGISTER_UANGLEC = 0xFF;  // C Voltage Phase Angle

}  // namespace atm90e32
}  // namespace esphome
