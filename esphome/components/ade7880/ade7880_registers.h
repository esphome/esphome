#pragma once

// This file is a modified version of the one created by Michaël Piron (@michaelpiron on GitHub)

// Source: https://www.analog.com/media/en/technical-documentation/application-notes/AN-1127.pdf

namespace esphome {
namespace ade7880 {

// DSP Data Memory RAM registers
constexpr uint16_t AIGAIN = 0x4380;
constexpr uint16_t AVGAIN = 0x4381;
constexpr uint16_t BIGAIN = 0x4382;
constexpr uint16_t BVGAIN = 0x4383;
constexpr uint16_t CIGAIN = 0x4384;
constexpr uint16_t CVGAIN = 0x4385;
constexpr uint16_t NIGAIN = 0x4386;

constexpr uint16_t DICOEFF = 0x4388;

constexpr uint16_t APGAIN = 0x4389;
constexpr uint16_t AWATTOS = 0x438A;
constexpr uint16_t BPGAIN = 0x438B;
constexpr uint16_t BWATTOS = 0x438C;
constexpr uint16_t CPGAIN = 0x438D;
constexpr uint16_t CWATTOS = 0x438E;
constexpr uint16_t AIRMSOS = 0x438F;
constexpr uint16_t AVRMSOS = 0x4390;
constexpr uint16_t BIRMSOS = 0x4391;
constexpr uint16_t BVRMSOS = 0x4392;
constexpr uint16_t CIRMSOS = 0x4393;
constexpr uint16_t CVRMSOS = 0x4394;
constexpr uint16_t NIRMSOS = 0x4395;
constexpr uint16_t HPGAIN = 0x4398;
constexpr uint16_t ISUMLVL = 0x4399;

constexpr uint16_t VLEVEL = 0x439F;

constexpr uint16_t AFWATTOS = 0x43A2;
constexpr uint16_t BFWATTOS = 0x43A3;
constexpr uint16_t CFWATTOS = 0x43A4;

constexpr uint16_t AFVAROS = 0x43A5;
constexpr uint16_t BFVAROS = 0x43A6;
constexpr uint16_t CFVAROS = 0x43A7;

constexpr uint16_t AFIRMSOS = 0x43A8;
constexpr uint16_t BFIRMSOS = 0x43A9;
constexpr uint16_t CFIRMSOS = 0x43AA;

constexpr uint16_t AFVRMSOS = 0x43AB;
constexpr uint16_t BFVRMSOS = 0x43AC;
constexpr uint16_t CFVRMSOS = 0x43AD;

constexpr uint16_t HXWATTOS = 0x43AE;
constexpr uint16_t HYWATTOS = 0x43AF;
constexpr uint16_t HZWATTOS = 0x43B0;
constexpr uint16_t HXVAROS = 0x43B1;
constexpr uint16_t HYVAROS = 0x43B2;
constexpr uint16_t HZVAROS = 0x43B3;

constexpr uint16_t HXIRMSOS = 0x43B4;
constexpr uint16_t HYIRMSOS = 0x43B5;
constexpr uint16_t HZIRMSOS = 0x43B6;
constexpr uint16_t HXVRMSOS = 0x43B7;
constexpr uint16_t HYVRMSOS = 0x43B8;
constexpr uint16_t HZVRMSOS = 0x43B9;

constexpr uint16_t AIRMS = 0x43C0;
constexpr uint16_t AVRMS = 0x43C1;
constexpr uint16_t BIRMS = 0x43C2;
constexpr uint16_t BVRMS = 0x43C3;
constexpr uint16_t CIRMS = 0x43C4;
constexpr uint16_t CVRMS = 0x43C5;
constexpr uint16_t NIRMS = 0x43C6;

constexpr uint16_t ISUM = 0x43C7;

// Internal DSP Memory RAM registers
constexpr uint16_t RUN = 0xE228;

constexpr uint16_t AWATTHR = 0xE400;
constexpr uint16_t BWATTHR = 0xE401;
constexpr uint16_t CWATTHR = 0xE402;
constexpr uint16_t AFWATTHR = 0xE403;
constexpr uint16_t BFWATTHR = 0xE404;
constexpr uint16_t CFWATTHR = 0xE405;
constexpr uint16_t AFVARHR = 0xE409;
constexpr uint16_t BFVARHR = 0xE40A;
constexpr uint16_t CFVARHR = 0xE40B;

constexpr uint16_t AVAHR = 0xE40C;
constexpr uint16_t BVAHR = 0xE40D;
constexpr uint16_t CVAHR = 0xE40E;

constexpr uint16_t IPEAK = 0xE500;
constexpr uint16_t VPEAK = 0xE501;

constexpr uint16_t STATUS0 = 0xE502;
constexpr uint16_t STATUS1 = 0xE503;

constexpr uint16_t AIMAV = 0xE504;
constexpr uint16_t BIMAV = 0xE505;
constexpr uint16_t CIMAV = 0xE506;

constexpr uint16_t OILVL = 0xE507;
constexpr uint16_t OVLVL = 0xE508;
constexpr uint16_t SAGLVL = 0xE509;
constexpr uint16_t MASK0 = 0xE50A;
constexpr uint16_t MASK1 = 0xE50B;

constexpr uint16_t IAWV = 0xE50C;
constexpr uint16_t IBWV = 0xE50D;
constexpr uint16_t ICWV = 0xE50E;
constexpr uint16_t INWV = 0xE50F;
constexpr uint16_t VAWV = 0xE510;
constexpr uint16_t VBWV = 0xE511;
constexpr uint16_t VCWV = 0xE512;

constexpr uint16_t AWATT = 0xE513;
constexpr uint16_t BWATT = 0xE514;
constexpr uint16_t CWATT = 0xE515;

constexpr uint16_t AFVAR = 0xE516;
constexpr uint16_t BFVAR = 0xE517;
constexpr uint16_t CFVAR = 0xE518;

constexpr uint16_t AVA = 0xE519;
constexpr uint16_t BVA = 0xE51A;
constexpr uint16_t CVA = 0xE51B;

constexpr uint16_t CHECKSUM = 0xE51F;
constexpr uint16_t VNOM = 0xE520;
constexpr uint16_t LAST_RWDATA_24BIT = 0xE5FF;
constexpr uint16_t PHSTATUS = 0xE600;
constexpr uint16_t ANGLE0 = 0xE601;
constexpr uint16_t ANGLE1 = 0xE602;
constexpr uint16_t ANGLE2 = 0xE603;
constexpr uint16_t PHNOLOAD = 0xE608;
constexpr uint16_t LINECYC = 0xE60C;
constexpr uint16_t ZXTOUT = 0xE60D;
constexpr uint16_t COMPMODE = 0xE60E;
constexpr uint16_t GAIN = 0xE60F;
constexpr uint16_t CFMODE = 0xE610;
constexpr uint16_t CF1DEN = 0xE611;
constexpr uint16_t CF2DEN = 0xE612;
constexpr uint16_t CF3DEN = 0xE613;
constexpr uint16_t APHCAL = 0xE614;
constexpr uint16_t BPHCAL = 0xE615;
constexpr uint16_t CPHCAL = 0xE616;
constexpr uint16_t PHSIGN = 0xE617;
constexpr uint16_t CONFIG = 0xE618;
constexpr uint16_t MMODE = 0xE700;
constexpr uint16_t ACCMODE = 0xE701;
constexpr uint16_t LCYCMODE = 0xE702;
constexpr uint16_t PEAKCYC = 0xE703;
constexpr uint16_t SAGCYC = 0xE704;
constexpr uint16_t CFCYC = 0xE705;
constexpr uint16_t HSDC_CFG = 0xE706;
constexpr uint16_t VERSION = 0xE707;
constexpr uint16_t DSPWP_SET = 0xE7E3;
constexpr uint16_t LAST_RWDATA_8BIT = 0xE7FD;
constexpr uint16_t DSPWP_SEL = 0xE7FE;
constexpr uint16_t FVRMS = 0xE880;
constexpr uint16_t FIRMS = 0xE881;
constexpr uint16_t FWATT = 0xE882;
constexpr uint16_t FVAR = 0xE883;
constexpr uint16_t FVA = 0xE884;
constexpr uint16_t FPF = 0xE885;
constexpr uint16_t VTHDN = 0xE886;
constexpr uint16_t ITHDN = 0xE887;
constexpr uint16_t HXVRMS = 0xE888;
constexpr uint16_t HXIRMS = 0xE889;
constexpr uint16_t HXWATT = 0xE88A;
constexpr uint16_t HXVAR = 0xE88B;
constexpr uint16_t HXVA = 0xE88C;
constexpr uint16_t HXPF = 0xE88D;
constexpr uint16_t HXVHD = 0xE88E;
constexpr uint16_t HXIHD = 0xE88F;
constexpr uint16_t HYVRMS = 0xE890;
constexpr uint16_t HYIRMS = 0xE891;
constexpr uint16_t HYWATT = 0xE892;
constexpr uint16_t HYVAR = 0xE893;
constexpr uint16_t HYVA = 0xE894;
constexpr uint16_t HYPF = 0xE895;
constexpr uint16_t HYVHD = 0xE896;
constexpr uint16_t HYIHD = 0xE897;
constexpr uint16_t HZVRMS = 0xE898;
constexpr uint16_t HZIRMS = 0xE899;
constexpr uint16_t HZWATT = 0xE89A;
constexpr uint16_t HZVAR = 0xE89B;
constexpr uint16_t HZVA = 0xE89C;
constexpr uint16_t HZPF = 0xE89D;
constexpr uint16_t HZVHD = 0xE89E;
constexpr uint16_t HZIHD = 0xE89F;
constexpr uint16_t HCONFIG = 0xE900;
constexpr uint16_t APF = 0xE902;
constexpr uint16_t BPF = 0xE903;
constexpr uint16_t CPF = 0xE904;
constexpr uint16_t APERIOD = 0xE905;
constexpr uint16_t BPERIOD = 0xE906;
constexpr uint16_t CPERIOD = 0xE907;
constexpr uint16_t APNOLOAD = 0xE908;
constexpr uint16_t VARNOLOAD = 0xE909;
constexpr uint16_t VANOLOAD = 0xE90A;
constexpr uint16_t LAST_ADD = 0xE9FE;
constexpr uint16_t LAST_RWDATA_16BIT = 0xE9FF;
constexpr uint16_t CONFIG3 = 0xEA00;
constexpr uint16_t LAST_OP = 0xEA01;
constexpr uint16_t WTHR = 0xEA02;
constexpr uint16_t VARTHR = 0xEA03;
constexpr uint16_t VATHR = 0xEA04;

constexpr uint16_t HX_REG = 0xEA08;
constexpr uint16_t HY_REG = 0xEA09;
constexpr uint16_t HZ_REG = 0xEA0A;
constexpr uint16_t LPOILVL = 0xEC00;
constexpr uint16_t CONFIG2 = 0xEC01;

// STATUS1 Register Bits
constexpr uint32_t STATUS1_RSTDONE = (1 << 15);

// CONFIG Register Bits
constexpr uint16_t CONFIG_SWRST = (1 << 7);

// CONFIG2 Register Bits
constexpr uint8_t CONFIG2_I2C_LOCK = (1 << 1);

// COMPMODE Register Bits
constexpr uint16_t COMPMODE_DEFAULT = 0x01FF;
constexpr uint16_t COMPMODE_SELFREQ = (1 << 14);

// RUN Register Bits
constexpr uint16_t RUN_ENABLE = (1 << 0);

// DSPWP_SET Register Bits
constexpr uint8_t DSPWP_SET_RO = (1 << 7);

// DSPWP_SEL Register Bits
constexpr uint8_t DSPWP_SEL_SET = 0xAD;

}  // namespace ade7880
}  // namespace esphome
