
/*
 Copyright (C) 2018-2022 Fredrik Öhrström (gpl-3.0-or-later)

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "dvparser.h"
#include"utils.h"

#include<assert.h>
#include<cmath>
#include<math.h>
#include<memory.h>
#include<limits>
#include "types.h"

// The parser should not crash on invalid data, but yeah, when I
// need to debug it because it crashes on invalid data, then
// I enable the following define...
//#define DEBUG_PARSER(...) fprintf(stdout, __VA_ARGS__)
#define DEBUG_PARSER(...)

using namespace std;

union RealConversion
{
    uint32_t i;
    float f;
};

const char* toString(VIFRange v)
{
    switch (v) {
    case VIFRange::None: return "None";
    case VIFRange::Any: return "Any";
#define X(name,from,to,quantity,unit) case VIFRange::name: return #name;
        LIST_OF_VIF_RANGES
#undef X
    }
    assert(0);
}

VIFRange toVIFRange(const char* s)
{
    if (!strcmp(s, "None")) return VIFRange::None;
    if (!strcmp(s, "Any")) return VIFRange::Any;
#define X(name,from,to,quantity,unit) if (!strcmp(s, #name)) return VIFRange::name;
    LIST_OF_VIF_RANGES
#undef X

        return VIFRange::None;
}

const char* toString(VIFCombinable v)
{
    switch (v) {
    case VIFCombinable::None: return "None";
    case VIFCombinable::Any: return "Any";
#define X(name,from,to) case VIFCombinable::name: return #name;
        LIST_OF_VIF_COMBINABLES
#undef X
    }
    assert(0);
}

std::string measurementTypeName(MeasurementType mt)
{
    switch (mt) {
    case MeasurementType::Any: return "any";
    case MeasurementType::Instantaneous: return "instantaneous";
    case MeasurementType::Maximum: return "maximum";
    case MeasurementType::Minimum: return "minimum";
    case MeasurementType::AtError: return "aterror";
    case MeasurementType::Unknown: return "unknown";
    }
    return "unknown";
}


VIFCombinable toVIFCombinable(int i)
{
#define X(name,from,to) if (from <= i && i <= to) return VIFCombinable::name;
    LIST_OF_VIF_COMBINABLES
#undef X
        return VIFCombinable::None;
}

Unit toDefaultUnit(Vif v)
{
#define X(name,from,to,quantity,unit) { if (from <= v.intValue() && v.intValue() <= to) return unit; }
    LIST_OF_VIF_RANGES
#undef X
        return Unit::Unknown;
}

Unit toDefaultUnit(VIFRange v)
{
    switch (v) {
    case VIFRange::Any:
    case VIFRange::None:
        assert(0);
        break;
#define X(name,from,to,quantity,unit) case VIFRange::name: return unit;
        LIST_OF_VIF_RANGES
#undef X
    }
    assert(0);
}

VIFRange toVIFRange(int i)
{
#define X(name,from,to,quantity,unit) if (from <= i && i <= to) return VIFRange::name;
    LIST_OF_VIF_RANGES
#undef X
        return VIFRange::None;
}

bool isInsideVIFRange(Vif vif, VIFRange vif_range)
{
    if (vif_range == VIFRange::AnyVolumeVIF)
    {
        // There are more volume units in the standard that will be added here.
        return isInsideVIFRange(vif, VIFRange::Volume);
    }
    if (vif_range == VIFRange::AnyEnergyVIF)
    {
        return
            isInsideVIFRange(vif, VIFRange::EnergyWh) ||
            isInsideVIFRange(vif, VIFRange::EnergyMJ) ||
            isInsideVIFRange(vif, VIFRange::EnergyMWh);
    }
    if (vif_range == VIFRange::AnyPowerVIF)
    {
        // There are more power units in the standard that will be added here.
        return isInsideVIFRange(vif, VIFRange::PowerW);
    }

#define X(name,from,to,quantity,unit) if (VIFRange::name == vif_range) { return from <= vif.intValue() && vif.intValue() <= to; }
    LIST_OF_VIF_RANGES
#undef X
        return false;
}

 std::map<uint16_t, string> hash_to_format_;

bool loadFormatBytesFromSignature(uint16_t format_signature, vector<uchar>* format_bytes)
{
    if (hash_to_format_.count(format_signature) > 0) {
        debug("(dvparser) found remembered format for hash %x", format_signature);
        // Return the proper hash!
        hex2bin(hash_to_format_[format_signature], format_bytes);
        return true;
    }
    // Unknown format signature.
    return false;
}

MeasurementType difMeasurementType(int dif)
{
    int t = dif & 0x30;
    switch (t) {
    case 0x00: return MeasurementType::Instantaneous;
    case 0x10: return MeasurementType::Maximum;
    case 0x20: return MeasurementType::Minimum;
    case 0x30: return MeasurementType::AtError;
    }
    assert(0);
}


int difLenBytes(int dif)
{
    int t = dif & 0x0f;
    switch (t) {
    case 0x0: return 0; // No data
    case 0x1: return 1; // 8 Bit Integer/Binary
    case 0x2: return 2; // 16 Bit Integer/Binary
    case 0x3: return 3; // 24 Bit Integer/Binary
    case 0x4: return 4; // 32 Bit Integer/Binary
    case 0x5: return 4; // 32 Bit Real
    case 0x6: return 6; // 48 Bit Integer/Binary
    case 0x7: return 8; // 64 Bit Integer/Binary
    case 0x8: return 0; // Selection for Readout
    case 0x9: return 1; // 2 digit BCD
    case 0xA: return 2; // 4 digit BCD
    case 0xB: return 3; // 6 digit BCD
    case 0xC: return 4; // 8 digit BCD
    case 0xD: return -1; // variable length
    case 0xE: return 6; // 12 digit BCD
    case 0xF: // Special Functions
        if (dif == 0x2f) return 1; // The skip code 0x2f, used for padding.
        return -2;
    }
    // Bad!
    return -2;
}


string vifType(int vif)
{
    // Remove any remaining 0x80 top bits.
    vif &= 0x7f7f;

    switch (vif)
    {
    case 0x00: return "Energy mWh";
    case 0x01: return "Energy 10⁻² Wh";
    case 0x02: return "Energy 10⁻¹ Wh";
    case 0x03: return "Energy Wh";
    case 0x04: return "Energy 10¹ Wh";
    case 0x05: return "Energy 10² Wh";
    case 0x06: return "Energy kWh";
    case 0x07: return "Energy 10⁴ Wh";

    case 0x08: return "Energy J";
    case 0x09: return "Energy 10¹ J";
    case 0x0A: return "Energy 10² J";
    case 0x0B: return "Energy kJ";
    case 0x0C: return "Energy 10⁴ J";
    case 0x0D: return "Energy 10⁵ J";
    case 0x0E: return "Energy MJ";
    case 0x0F: return "Energy 10⁷ J";

    case 0x10: return "Volume cm³";
    case 0x11: return "Volume 10⁻⁵ m³";
    case 0x12: return "Volume 10⁻⁴ m³";
    case 0x13: return "Volume l";
    case 0x14: return "Volume 10⁻² m³";
    case 0x15: return "Volume 10⁻¹ m³";
    case 0x16: return "Volume m³";
    case 0x17: return "Volume 10¹ m³";

    case 0x18: return "Mass g";
    case 0x19: return "Mass 10⁻² kg";
    case 0x1A: return "Mass 10⁻¹ kg";
    case 0x1B: return "Mass kg";
    case 0x1C: return "Mass 10¹ kg";
    case 0x1D: return "Mass 10² kg";
    case 0x1E: return "Mass t";
    case 0x1F: return "Mass 10⁴ kg";

    case 0x20: return "On time seconds";
    case 0x21: return "On time minutes";
    case 0x22: return "On time hours";
    case 0x23: return "On time days";

    case 0x24: return "Operating time seconds";
    case 0x25: return "Operating time minutes";
    case 0x26: return "Operating time hours";
    case 0x27: return "Operating time days";

    case 0x28: return "Power mW";
    case 0x29: return "Power 10⁻² W";
    case 0x2A: return "Power 10⁻¹ W";
    case 0x2B: return "Power W";
    case 0x2C: return "Power 10¹ W";
    case 0x2D: return "Power 10² W";
    case 0x2E: return "Power kW";
    case 0x2F: return "Power 10⁴ W";

    case 0x30: return "Power J/h";
    case 0x31: return "Power 10¹ J/h";
    case 0x32: return "Power 10² J/h";
    case 0x33: return "Power kJ/h";
    case 0x34: return "Power 10⁴ J/h";
    case 0x35: return "Power 10⁵ J/h";
    case 0x36: return "Power MJ/h";
    case 0x37: return "Power 10⁷ J/h";

    case 0x38: return "Volume flow cm³/h";
    case 0x39: return "Volume flow 10⁻⁵ m³/h";
    case 0x3A: return "Volume flow 10⁻⁴ m³/h";
    case 0x3B: return "Volume flow l/h";
    case 0x3C: return "Volume flow 10⁻² m³/h";
    case 0x3D: return "Volume flow 10⁻¹ m³/h";
    case 0x3E: return "Volume flow m³/h";
    case 0x3F: return "Volume flow 10¹ m³/h";

    case 0x40: return "Volume flow ext. 10⁻⁷ m³/min";
    case 0x41: return "Volume flow ext. cm³/min";
    case 0x42: return "Volume flow ext. 10⁻⁵ m³/min";
    case 0x43: return "Volume flow ext. 10⁻⁴ m³/min";
    case 0x44: return "Volume flow ext. l/min";
    case 0x45: return "Volume flow ext. 10⁻² m³/min";
    case 0x46: return "Volume flow ext. 10⁻¹ m³/min";
    case 0x47: return "Volume flow ext. m³/min";

    case 0x48: return "Volume flow ext. mm³/s";
    case 0x49: return "Volume flow ext. 10⁻⁸ m³/s";
    case 0x4A: return "Volume flow ext. 10⁻⁷ m³/s";
    case 0x4B: return "Volume flow ext. cm³/s";
    case 0x4C: return "Volume flow ext. 10⁻⁵ m³/s";
    case 0x4D: return "Volume flow ext. 10⁻⁴ m³/s";
    case 0x4E: return "Volume flow ext. l/s";
    case 0x4F: return "Volume flow ext. 10⁻² m³/s";

    case 0x50: return "Mass g/h";
    case 0x51: return "Mass 10⁻² kg/h";
    case 0x52: return "Mass 10⁻¹ kg/h";
    case 0x53: return "Mass kg/h";
    case 0x54: return "Mass 10¹ kg/h";
    case 0x55: return "Mass 10² kg/h";
    case 0x56: return "Mass t/h";
    case 0x57: return "Mass 10⁴ kg/h";

    case 0x58: return "Flow temperature 10⁻³ °C";
    case 0x59: return "Flow temperature 10⁻² °C";
    case 0x5A: return "Flow temperature 10⁻¹ °C";
    case 0x5B: return "Flow temperature °C";

    case 0x5C: return "Return temperature 10⁻³ °C";
    case 0x5D: return "Return temperature 10⁻² °C";
    case 0x5E: return "Return temperature 10⁻¹ °C";
    case 0x5F: return "Return temperature °C";

    case 0x60: return "Temperature difference 10⁻³ K/°C";
    case 0x61: return "Temperature difference 10⁻² K/°C";
    case 0x62: return "Temperature difference 10⁻¹ K/°C";
    case 0x63: return "Temperature difference K/°C";

    case 0x64: return "External temperature 10⁻³ °C";
    case 0x65: return "External temperature 10⁻² °C";
    case 0x66: return "External temperature 10⁻¹ °C";
    case 0x67: return "External temperature °C";

    case 0x68: return "Pressure mbar";
    case 0x69: return "Pressure 10⁻² bar";
    case 0x6A: return "Pressure 10⁻¹ bar";
    case 0x6B: return "Pressure bar";

    case 0x6C: return "Date type G";
    case 0x6D: return "Date and time type";

    case 0x6E: return "Units for H.C.A.";
    case 0x6F: return "Third extension 6F of VIF-codes";

    case 0x70: return "Averaging duration seconds";
    case 0x71: return "Averaging duration minutes";
    case 0x72: return "Averaging duration hours";
    case 0x73: return "Averaging duration days";

    case 0x74: return "Actuality duration seconds";
    case 0x75: return "Actuality duration minutes";
    case 0x76: return "Actuality duration hours";
    case 0x77: return "Actuality duration days";

    case 0x78: return "Fabrication no";
    case 0x79: return "Enhanced identification";

    case 0x7B: return "First extension FB of VIF-codes";
    case 0x7C: return "VIF in following string (length in first byte)";
    case 0x7D: return "Second extension FD of VIF-codes";

    case 0x7E: return "Any VIF";
    case 0x7F: return "Manufacturer specific";

    case 0x7B00: return "Active Energy 0.1 MWh";
    case 0x7B01: return "Active Energy 1 MWh";

    case 0x7B1A: return "Relative humidity 0.1%";
    case 0x7B1B: return "Relative humidity 1%";

    default: return "?";
    }
    return "?";
}


string vif_7B_FirstExtensionType(uchar dif, uchar vif, uchar vife)
{
    assert(vif == 0xfb);

    if ((vife & 0x7e) == 0x00) {
        int n = vife & 0x01;
        string s;
        strprintf(&s, "10^%d MWh", n - 1);
        return s;
    }

    if (((vife & 0x7e) == 0x02) ||
        ((vife & 0x7c) == 0x04)) {
        return "Reserved";
    }

    if ((vife & 0x7e) == 0x08) {
        int n = vife & 0x01;
        string s;
        strprintf(&s, "10^%d GJ", n - 1);
        return s;
    }

    if ((vife & 0x7e) == 0x0a ||
        (vife & 0x7c) == 0x0c) {
        return "Reserved";
    }

    if ((vife & 0x7e) == 0x10) {
        int n = vife & 0x01;
        string s;
        strprintf(&s, "10^%d m3", n + 2);
        return s;
    }

    if ((vife & 0x7e) == 0x12 ||
        (vife & 0x7c) == 0x14) {
        return "Reserved";
    }

    if ((vife & 0x7e) == 0x18) {
        int n = vife & 0x01;
        string s;
        strprintf(&s, "10^%d ton", n + 2);
        return s;
    }

    if ((vife & 0x7e) == 0x1a) {
        int n = vife & 0x01;
        string s;
        strprintf(&s, "Relative Humidity 10^%d %%", n - 1);
        return s;
    }

    if ((vif & 0x7e) >= 0x1a && (vif & 0x7e) <= 0x20) {
        return "Reserved";
    }

    if ((vife & 0x7f) == 0x21) {
        return "0.1 feet^3";
    }

    if ((vife & 0x7f) == 0x22) {
        return "0.1 american gallon";
    }

    if ((vife & 0x7f) == 0x23) {
        return "american gallon";
    }

    if ((vife & 0x7f) == 0x24) {
        return "0.001 american gallon/min";
    }

    if ((vife & 0x7f) == 0x25) {
        return "american gallon/min";
    }

    if ((vife & 0x7f) == 0x26) {
        return "american gallon/h";
    }

    if ((vife & 0x7f) == 0x27) {
        return "Reserved";
    }

    if ((vife & 0x7f) == 0x20) {
        return "Volume feet";
    }

    if ((vife & 0x7f) == 0x21) {
        return "Volume 0.1 feet";
    }

    if ((vife & 0x7e) == 0x28) {
        // Come again? A unit of 1MW...do they intend to use m-bus to track the
        // output from a nuclear power plant?
        int n = vife & 0x01;
        string s;
        strprintf(&s, "10^%d MW", n - 1);
        return s;
    }

    if ((vife & 0x7f) == 0x29 ||
        (vife & 0x7c) == 0x2c) {
        return "Reserved";
    }

    if ((vife & 0x7e) == 0x30) {
        int n = vife & 0x01;
        string s;
        strprintf(&s, "10^%d GJ/h", n - 1);
        return s;
    }

    if ((vife & 0x7f) >= 0x32 && (vife & 0x7c) <= 0x57) {
        return "Reserved";
    }

    if ((vife & 0x7c) == 0x58) {
        int nn = vife & 0x03;
        string s;
        strprintf(&s, "Flow temperature 10^%d Fahrenheit", nn - 3);
        return s;
    }

    if ((vife & 0x7c) == 0x5c) {
        int nn = vife & 0x03;
        string s;
        strprintf(&s, "Return temperature 10^%d Fahrenheit", nn - 3);
        return s;
    }

    if ((vife & 0x7c) == 0x60) {
        int nn = vife & 0x03;
        string s;
        strprintf(&s, "Temperature difference 10^%d Fahrenheit", nn - 3);
        return s;
    }

    if ((vife & 0x7c) == 0x64) {
        int nn = vife & 0x03;
        string s;
        strprintf(&s, "External temperature 10^%d Fahrenheit", nn - 3);
        return s;
    }

    if ((vife & 0x78) == 0x68) {
        return "Reserved";
    }

    if ((vife & 0x7c) == 0x70) {
        int nn = vife & 0x03;
        string s;
        strprintf(&s, "Cold / Warm Temperature Limit 10^%d Fahrenheit", nn - 3);
        return s;
    }

    if ((vife & 0x7c) == 0x74) {
        int nn = vife & 0x03;
        string s;
        strprintf(&s, "Cold / Warm Temperature Limit 10^%d Celsius", nn - 3);
        return s;
    }

    if ((vife & 0x78) == 0x78) {
        int nnn = vife & 0x07;
        string s;
        strprintf(&s, "Cumulative count max power 10^%d W", nnn - 3);
        return s;
    }

    return "?";
}




string vif_7D_SecondExtensionType(uchar dif, uchar vif, uchar vife)
{
    assert(vif == 0xfd);

    if ((vife & 0x7c) == 0x00) {
        int nn = vife & 0x03;
        string s;
        strprintf(&s, "Credit of 10^%d of the nominal local legal currency units", nn - 3);
        return s;
    }

    if ((vife & 0x7c) == 0x04) {
        int nn = vife & 0x03;
        string s;
        strprintf(&s, "Debit of 10^%d of the nominal local legal currency units", nn - 3);
        return s;
    }

    if ((vife & 0x7f) == 0x08) {
        return "Access Number (transmission count)";
    }

    if ((vife & 0x7f) == 0x09) {
        return "Medium (as in fixed header)";
    }

    if ((vife & 0x7f) == 0x0a) {
        return "Manufacturer (as in fixed header)";
    }

    if ((vife & 0x7f) == 0x0b) {
        return "Parameter set identification";
    }

    if ((vife & 0x7f) == 0x0c) {
        return "Model/Version";
    }

    if ((vife & 0x7f) == 0x0d) {
        return "Hardware version #";
    }

    if ((vife & 0x7f) == 0x0e) {
        return "Firmware version #";
    }

    if ((vife & 0x7f) == 0x0f) {
        return "Software version #";
    }

    if ((vife & 0x7f) == 0x10) {
        return "Customer location";
    }

    if ((vife & 0x7f) == 0x11) {
        return "Customer";
    }

    if ((vife & 0x7f) == 0x12) {
        return "Access Code User";
    }

    if ((vife & 0x7f) == 0x13) {
        return "Access Code Operator";
    }

    if ((vife & 0x7f) == 0x14) {
        return "Access Code System Operator";
    }

    if ((vife & 0x7f) == 0x15) {
        return "Access Code Developer";
    }

    if ((vife & 0x7f) == 0x16) {
        return "Password";
    }

    if ((vife & 0x7f) == 0x17) {
        return "Error flags (binary)";
    }

    if ((vife & 0x7f) == 0x18) {
        return "Error mask";
    }

    if ((vife & 0x7f) == 0x19) {
        return "Reserved";
    }

    if ((vife & 0x7f) == 0x1a) {
        return "Digital Output (binary)";
    }

    if ((vife & 0x7f) == 0x1b) {
        return "Digital Input (binary)";
    }

    if ((vife & 0x7f) == 0x1c) {
        return "Baudrate [Baud]";
    }

    if ((vife & 0x7f) == 0x1d) {
        return "Response delay time [bittimes]";
    }

    if ((vife & 0x7f) == 0x1e) {
        return "Retry";
    }

    if ((vife & 0x7f) == 0x1f) {
        return "Reserved";
    }

    if ((vife & 0x7f) == 0x20) {
        return "First storage # for cyclic storage";
    }

    if ((vife & 0x7f) == 0x21) {
        return "Last storage # for cyclic storage";
    }

    if ((vife & 0x7f) == 0x22) {
        return "Size of storage block";
    }

    if ((vife & 0x7f) == 0x23) {
        return "Reserved";
    }

    if ((vife & 0x7c) == 0x24) {
        int nn = vife & 0x03;
        string s;
        strprintf(&s, "Storage interval [%s]", timeNN(nn));
        return s;
    }

    if ((vife & 0x7f) == 0x28) {
        return "Storage interval month(s)";
    }

    if ((vife & 0x7f) == 0x29) {
        return "Storage interval year(s)";
    }

    if ((vife & 0x7f) == 0x2a) {
        return "Reserved";
    }

    if ((vife & 0x7f) == 0x2b) {
        return "Reserved";
    }

    if ((vife & 0x7c) == 0x2c) {
        int nn = vife & 0x03;
        string s;
        strprintf(&s, "Duration since last readout [%s]", timeNN(nn));
        return s;
    }

    if ((vife & 0x7f) == 0x30) {
        return "Start (date/time) of tariff";
    }

    if ((vife & 0x7c) == 0x30) {
        int nn = vife & 0x03;
        string s;
        // nn == 0 (seconds) is not expected here! According to m-bus spec.
        strprintf(&s, "Duration of tariff [%s]", timeNN(nn));
        return s;
    }

    if ((vife & 0x7c) == 0x34) {
        int nn = vife & 0x03;
        string s;
        strprintf(&s, "Period of tariff [%s]", timeNN(nn));
        return s;
    }

    if ((vife & 0x7f) == 0x38) {
        return "Period of tariff months(s)";
    }

    if ((vife & 0x7f) == 0x39) {
        return "Period of tariff year(s)";
    }

    if ((vife & 0x7f) == 0x3a) {
        return "Dimensionless / no VIF";
    }

    if ((vife & 0x7f) == 0x3b) {
        return "Reserved";
    }

    if ((vife & 0x7c) == 0x3c) {
        // int xx = vife & 0x03;
        return "Reserved";
    }

    if ((vife & 0x70) == 0x40) {
        int nnnn = vife & 0x0f;
        string s;
        strprintf(&s, "10^%d Volts", nnnn - 9);
        return s;
    }

    if ((vife & 0x70) == 0x50) {
        int nnnn = vife & 0x0f;
        string s;
        strprintf(&s, "10^%d Ampere", nnnn - 12);
        return s;
    }

    if ((vife & 0x7f) == 0x60) {
        return "Reset counter";
    }

    if ((vife & 0x7f) == 0x61) {
        return "Cumulation counter";
    }

    if ((vife & 0x7f) == 0x62) {
        return "Control signal";
    }

    if ((vife & 0x7f) == 0x63) {
        return "Day of week";
    }

    if ((vife & 0x7f) == 0x64) {
        return "Week number";
    }

    if ((vife & 0x7f) == 0x65) {
        return "Time point of day change";
    }

    if ((vife & 0x7f) == 0x66) {
        return "State of parameter activation";
    }

    if ((vife & 0x7f) == 0x67) {
        return "Special supplier information";
    }

    if ((vife & 0x7c) == 0x68) {
        int pp = vife & 0x03;
        string s;
        strprintf(&s, "Duration since last cumulation [%s]", timePP(pp));
        return s;
    }

    if ((vife & 0x7c) == 0x6c) {
        int pp = vife & 0x03;
        string s;
        strprintf(&s, "Operating time battery [%s]", timePP(pp));
        return s;
    }

    if ((vife & 0x7f) == 0x70) {
        return "Date and time of battery change";
    }

    if ((vife & 0x7f) >= 0x71) {
        return "Reserved";
    }

    if ((vife & 0x7f) == 0x74) {
        return "Remaining battery in days";
    }

    return "?";
}


string vifeType(int dif, int vif, int vife)
{
    if (vif == 0xfb) { // 0x7b without high bit
        return vif_7B_FirstExtensionType(dif, vif, vife);
    }
    if (vif == 0xfd) { // 0x7d without high bit
        return vif_7D_SecondExtensionType(dif, vif, vife);
    }
    if (vif == 0xef) { // 0x6f without high bit
        return vif_6F_ThirdExtensionType(dif, vif, vife);
    }
    if (vif == 0xff) { // 0x7f without high bit
        return vif_7F_ManufacturerExtensionType(dif, vif, vife);
    }
    vife = vife & 0x7f; // Strip the bit signifying more vifes after this.
    if (vife == 0x1f) {
        return "Compact profile without register";
    }
    if (vife == 0x13) {
        return "Reverse compact profile without register";
    }
    if (vife == 0x1e) {
        return "Compact profile with register";
    }
    if (vife == 0x20) {
        return "per second";
    }
    if (vife == 0x21) {
        return "per minute";
    }
    if (vife == 0x22) {
        return "per hour";
    }
    if (vife == 0x23) {
        return "per day";
    }
    if (vife == 0x24) {
        return "per week";
    }
    if (vife == 0x25) {
        return "per month";
    }
    if (vife == 0x26) {
        return "per year";
    }
    if (vife == 0x27) {
        return "per revolution/measurement";
    }
    if (vife == 0x28) {
        return "incr per input pulse on input channel 0";
    }
    if (vife == 0x29) {
        return "incr per input pulse on input channel 1";
    }
    if (vife == 0x2a) {
        return "incr per output pulse on input channel 0";
    }
    if (vife == 0x2b) {
        return "incr per output pulse on input channel 1";
    }
    if (vife == 0x2c) {
        return "per litre";
    }
    if (vife == 0x2d) {
        return "per m3";
    }
    if (vife == 0x2e) {
        return "per kg";
    }
    if (vife == 0x2f) {
        return "per kelvin";
    }
    if (vife == 0x30) {
        return "per kWh";
    }
    if (vife == 0x31) {
        return "per GJ";
    }
    if (vife == 0x32) {
        return "per kW";
    }
    if (vife == 0x33) {
        return "per kelvin*litre";
    }
    if (vife == 0x34) {
        return "per volt";
    }
    if (vife == 0x35) {
        return "per ampere";
    }
    if (vife == 0x36) {
        return "multiplied by s";
    }
    if (vife == 0x37) {
        return "multiplied by s/V";
    }
    if (vife == 0x38) {
        return "multiplied by s/A";
    }
    if (vife == 0x39) {
        return "start date/time of a,b";
    }
    if (vife == 0x3a) {
        return "uncorrected meter unit";
    }
    if (vife == 0x3b) {
        return "forward flow";
    }
    if (vife == 0x3c) {
        return "backward flow";
    }
    if (vife == 0x3d) {
        return "reserved for non-metric unit systems";
    }
    if (vife == 0x3e) {
        return "value at base conditions c";
    }
    if (vife == 0x3f) {
        return "obis-declaration";
    }
    if (vife == 0x40) {
        return "obis-declaration";
    }
    if (vife == 0x40) {
        return "lower limit";
    }
    if (vife == 0x48) {
        return "upper limit";
    }
    if (vife == 0x41) {
        return "number of exceeds of lower limit";
    }
    if (vife == 0x49) {
        return "number of exceeds of upper limit";
    }
    if ((vife & 0x72) == 0x42) {
        string msg = "date/time of ";
        if (vife & 0x01) msg += "end ";
        else msg += "beginning ";
        msg += " of ";
        if (vife & 0x04) msg += "last ";
        else msg += "first ";
        if (vife & 0x08) msg += "upper ";
        else msg += "lower ";
        msg += "limit exceed";
        return msg;
    }
    if ((vife & 0x70) == 0x50) {
        string msg = "duration of limit exceed ";
        if (vife & 0x04) msg += "last ";
        else msg += "first ";
        if (vife & 0x08) msg += "upper ";
        else msg += "lower ";
        int nn = vife & 0x03;
        msg += " is " + std::to_string(nn);
        return msg;
    }
    if ((vife & 0x78) == 0x60) {
        string msg = "duration of a,b ";
        if (vife & 0x04) msg += "last ";
        else msg += "first ";
        int nn = vife & 0x03;
        msg += " is " + std::to_string(nn);
        return msg;
    }
    if ((vife & 0x7B) == 0x68) {
        string msg = "value during ";
        if (vife & 0x04) msg += "upper ";
        else msg += "lower ";
        msg += "limit exceed";
        return msg;
    }
    if (vife == 0x69) {
        return "leakage values";
    }
    if (vife == 0x6d) {
        return "overflow values";
    }
    if ((vife & 0x7a) == 0x6a) {
        string msg = "date/time of a: ";
        if (vife & 0x01) msg += "end ";
        else msg += "beginning ";
        msg += " of ";
        if (vife & 0x04) msg += "last ";
        else msg += "first ";
        if (vife & 0x08) msg += "upper ";
        else msg += "lower ";
        return msg;
    }
    if ((vife & 0x78) == 0x70) {
        int nnn = vife & 0x07;
        return "multiplicative correction factor: 10^" + std::to_string(nnn - 6);
    }
    if ((vife & 0x78) == 0x78) {
        int nn = vife & 0x03;
        return "additive correction constant: unit of VIF * 10^" + std::to_string(nn - 3);
    }
    if (vife == 0x7c) {
        return "extension of combinable vife";
    }
    if (vife == 0x7d) {
        return "multiplicative correction factor for value";
    }
    if (vife == 0x7e) {
        return "future value";
    }
    if (vif == 0x7f) {
        return "manufacturer specific";
    }
    return "?";
}


string difType(int dif)
{
    string s;
    int t = dif & 0x0f;
    switch (t) {
    case 0x0: s += "No data"; break;
    case 0x1: s += "8 Bit Integer/Binary"; break;
    case 0x2: s += "16 Bit Integer/Binary"; break;
    case 0x3: s += "24 Bit Integer/Binary"; break;
    case 0x4: s += "32 Bit Integer/Binary"; break;
    case 0x5: s += "32 Bit Real"; break;
    case 0x6: s += "48 Bit Integer/Binary"; break;
    case 0x7: s += "64 Bit Integer/Binary"; break;
    case 0x8: s += "Selection for Readout"; break;
    case 0x9: s += "2 digit BCD"; break;
    case 0xA: s += "4 digit BCD"; break;
    case 0xB: s += "6 digit BCD"; break;
    case 0xC: s += "8 digit BCD"; break;
    case 0xD: s += "variable length"; break;
    case 0xE: s += "12 digit BCD"; break;
    case 0xF: s += "Special Functions"; break;
    default: s += "?"; break;
    }

    if (t != 0xf)
    {
        // Only print these suffixes when we have actual values.
        t = dif & 0x30;

        switch (t) {
        case 0x00: s += " Instantaneous value"; break;
        case 0x10: s += " Maximum value"; break;
        case 0x20: s += " Minimum value"; break;
        case 0x30: s += " Value during error state"; break;
        default: s += "?"; break;
        }
    }
    if (dif & 0x40) {
        // This is the lsb of the storage nr.
        s += " storagenr=1";
    }
    return s;
}

bool parseDV(Telegram* t,
    vector<uchar>& databytes,
    vector<uchar>::iterator data,
    size_t data_len,
     std::map<string, pair<int, DVEntry>>* dv_entries,
    vector<uchar>::iterator* format,
    size_t format_len,
    uint16_t* format_hash)
{
     std::map<string, int> dv_count;
    vector<uchar> format_bytes;
    vector<uchar> id_bytes;
    vector<uchar> data_bytes;
    string dv, key;
    size_t start_parse_here = t->parsed.size();
    vector<uchar>::iterator data_start = data;
    vector<uchar>::iterator data_end = data + data_len;
    vector<uchar>::iterator format_end;
    bool data_has_difvifs = true;
    bool variable_length = false;
    int force_mfct_index = t->force_mfct_index;

    if (format == NULL) {
        // No format string was supplied, we therefore assume
        // that the difvifs necessary to parse the data is
        // part of the data! This is the default.
        format = &data;
        format_end = data_end;
    }
    else {
        // A format string has been supplied. The data is compressed,
        // and can only be decoded using the supplied difvifs.
        // Since the data does not have the difvifs.
        data_has_difvifs = false;
        format_end = *format + format_len;
        string s = bin2hex(*format, format_end, format_len);
        debug("(dvparser) using format \"%s\"", s.c_str());
    }

    dv_entries->clear();

    // Data format is:

    // DIF byte (defines how the binary data bits should be decoded and howy man data bytes there are)
    // Sometimes followed by one or more dife bytes, if the 0x80 high bit is set.
    // The last dife byte does not have the 0x80 bit set.

    // VIF byte (defines what the decoded value means, water,energy,power,etc.)
    // Sometimes followed by one or more vife bytes, if the 0x80 high bit is set.
    // The last vife byte does not have the 0x80 bit set.

    // Data bytes, the number of data bytes are defined by the dif format.
    // Or if the dif says variable length, then the first data byte specifies the number of data bytes.

    // DIF again...

    // A Dif(Difes)Vif(Vifes) identifier can be for example be the 02FF20 for the Multical21
    // vendor specific status bits. The parser then uses this identifier as a key to store the
    // data bytes in a map. The same identifier could occur several times in a telegram,
    // even though it often don't. Since the first occurence is stored under 02FF20,
    // the second identical identifier stores its data under the key "02FF20_2" etc for 3 and forth...
    // A proper meter would use storagenr etc to differentiate between different measurements of
    // the same value.

    format_bytes.clear();
    id_bytes.clear();
    for (;;)
    {
        id_bytes.clear();
        DEBUG_PARSER("(dvparser debug) Remaining format data %ju", std::distance(*format, format_end));
        if (*format == format_end) break;

        if (force_mfct_index != -1)
        {
            // This is an old meter without a proper 0f or other hear start manufacturer data marker.
            int index = std::distance(data_start, data);

            if (index >= force_mfct_index)
            {
                DEBUG_PARSER("(dvparser) manufacturer specific data, parsing is done.", dif);
                size_t datalen = std::distance(data, data_end);
                string value = bin2hex(data, data_end, datalen);
                t->addExplanationAndIncrementPos(data, datalen, KindOfData::CONTENT, Understanding::NONE, "manufacturer specific data %s", value.c_str());
                break;
            }
        }

        uchar dif = **format;

        MeasurementType mt = difMeasurementType(dif);
        int datalen = difLenBytes(dif);
        DEBUG_PARSER("(dvparser debug) dif=%02x datalen=%d \"%s\" type=%s", dif, datalen, difType(dif).c_str(),
            measurementTypeName(mt).c_str());

        if (datalen == -2)
        {
            if (dif == 0x0f)
            {
                DEBUG_PARSER("(dvparser) reached dif %02x manufacturer specific data, parsing is done.", dif);
                datalen = std::distance(data, data_end);
                string value = bin2hex(data + 1, data_end, datalen - 1);
                t->mfct_0f_index = 1 + std::distance(data_start, data);
                assert(t->mfct_0f_index >= 0);
                t->addExplanationAndIncrementPos(data, datalen, KindOfData::CONTENT, Understanding::NONE, "%02X manufacturer specific data %s", dif, value.c_str());
                break;
            }
            if (dif == 0x1f)
            {
                DEBUG_PARSER("(dvparser) reached dif %02x more records in next telegram.", dif);
                datalen = std::distance(data, data_end);
                string value = bin2hex(data + 1, data_end, datalen - 1);
                t->mfct_0f_index = 1 + std::distance(data_start, data);
                assert(t->mfct_0f_index >= 0);
                t->addExplanationAndIncrementPos(data, datalen, KindOfData::CONTENT, Understanding::FULL, "%02X more data in next telegram %s", dif, value.c_str());
                break;
            }
            DEBUG_PARSER("(dvparser) reached unknown dif %02x treating remaining data as manufacturer specific, parsing is done.", dif);
            datalen = std::distance(data, data_end);
            string value = bin2hex(data + 1, data_end, datalen - 1);
            t->mfct_0f_index = 1 + std::distance(data_start, data);
            assert(t->mfct_0f_index >= 0);
            t->addExplanationAndIncrementPos(data, datalen, KindOfData::CONTENT, Understanding::NONE, "%02X unknown dif treating remaining data as mfct specific %s", dif, value.c_str());
            break;
        }
        if (dif == 0x2f) {
            t->addExplanationAndIncrementPos(*format, 1, KindOfData::PROTOCOL, Understanding::FULL, "%02X skip", dif);
            continue;
        }
        if (datalen == -1) {
            variable_length = true;
        }
        else {
            variable_length = false;
        }
        if (data_has_difvifs) {
            format_bytes.push_back(dif);
            id_bytes.push_back(dif);
            t->addExplanationAndIncrementPos(*format, 1, KindOfData::PROTOCOL, Understanding::FULL, "%02X dif (%s)", dif, difType(dif).c_str());
        }
        else {
            id_bytes.push_back(**format);
            (*format)++;
        }


        int difenr = 0;
        int subunit = 0;
        int tariff = 0;
        int lsb_of_storage_nr = (dif & 0x40) >> 6;
        int storage_nr = lsb_of_storage_nr;

        bool has_another_dife = (dif & 0x80) == 0x80;

        while (has_another_dife)
        {
            if (*format == format_end) { debug("(dvparser) warning: unexpected end of data (dife expected)"); break; }

            uchar dife = **format;
            int subunit_bit = (dife & 0x40) >> 6;
            subunit |= subunit_bit << difenr;
            int tariff_bits = (dife & 0x30) >> 4;
            tariff |= tariff_bits << (difenr * 2);
            int storage_nr_bits = (dife & 0x0f);
            storage_nr |= storage_nr_bits << (1 + difenr * 4);

            DEBUG_PARSER("(dvparser debug) dife=%02x (subunit=%d tariff=%d storagenr=%d)", dife, subunit, tariff, storage_nr);

            if (data_has_difvifs)
            {
                format_bytes.push_back(dife);
                id_bytes.push_back(dife);
                t->addExplanationAndIncrementPos(*format, 1, KindOfData::PROTOCOL, Understanding::FULL,
                    "%02X dife (subunit=%d tariff=%d storagenr=%d)",
                    dife, subunit, tariff, storage_nr);
            }
            else
            {
                id_bytes.push_back(**format);
                (*format)++;
            }

            has_another_dife = (dife & 0x80) == 0x80;
            difenr++;
        }

        if (*format == format_end) { debug("(dvparser) warning: unexpected end of data (vif expected)"); break; }

        uchar vif = **format;
        int full_vif = vif & 0x7f;
        bool extension_vif = false;
        int combinable_full_vif = 0;
        bool combinable_extension_vif = false;
        std::set<VIFCombinable> found_combinable_vifs;
        std::set<uint16_t> found_combinable_vifs_raw;

        DEBUG_PARSER("(dvparser debug) vif=%04x \"%s\"", vif, vifType(vif).c_str());

        if (data_has_difvifs)
        {
            format_bytes.push_back(vif);
            id_bytes.push_back(vif);
            t->addExplanationAndIncrementPos(*format, 1, KindOfData::PROTOCOL, Understanding::FULL,
                "%02X vif (%s)", vif, vifType(vif).c_str());
        }
        else
        {
            id_bytes.push_back(**format);
            (*format)++;
        }

        // Check if this is marker for one of the extended sets of vifs: first, second and third or manufacturer.
        if (vif == 0xfb || vif == 0xfd || vif == 0xef || vif == 0xff)
        {
            // Extension vifs.
            full_vif <<= 8;
            extension_vif = true;
        }

        // Grabbing a variable length vif. This does not currently work
        // with the compact format.
        if (vif == 0x7c)
        {
            DEBUG_PARSER("(dvparser debug) variable length vif found");
            if (*format == format_end) { debug("(dvparser) warning: unexpected end of data (vif varlen expected)"); break; }
            uchar viflen = **format;
            id_bytes.push_back(viflen);
            t->addExplanationAndIncrementPos(*format, 1, KindOfData::PROTOCOL, Understanding::FULL,
                "%02X viflen (%d)", viflen, viflen);
            for (uchar i = 0; i < viflen; ++i)
            {
                if (*format == format_end) {
                    debug("(dvparser) warning: unexpected end of data (vif varlen byte %d/%d expected)",
                        i + 1, viflen); break;
                }
                uchar v = **format;
                t->addExplanationAndIncrementPos(*format, 1, KindOfData::PROTOCOL, Understanding::FULL,
                    "%02X vif (%c)", v, v);
                id_bytes.push_back(v);
            }
        }

        // Do we have another vife byte? We better have one, if extension_vif is true.
        bool has_another_vife = (vif & 0x80) == 0x80;
        while (has_another_vife)
        {
            if (*format == format_end) { debug("(dvparser) warning: unexpected end of data (vife expected)"); break; }

            uchar vife = **format;
            DEBUG_PARSER("(dvparser debug) vife=%02x (%s)", vife, vifeType(dif, vif, vife).c_str());

            if (data_has_difvifs)
            {
                // Collect the difvifs to create signature for future use.
                format_bytes.push_back(vife);
                id_bytes.push_back(vife);
            }
            else
            {
                // Reuse the existing
                id_bytes.push_back(**format);
                (*format)++;
            }

            has_another_vife = (vife & 0x80) == 0x80;

            if (extension_vif)
            {
                // First vife after the extension marker is the real vif.
                full_vif |= (vife & 0x7f);
                extension_vif = false;
                if (data_has_difvifs)
                {
                    t->addExplanationAndIncrementPos(*format, 1, KindOfData::PROTOCOL, Understanding::FULL,
                        "%02X vife (%s)", vife, vifeType(dif, vif, vife).c_str());
                }
            }
            else
            {
                if (combinable_extension_vif)
                {
                    // First vife after the combinable extension marker is the real vif.
                    combinable_full_vif |= (vife & 0x7f);
                    combinable_extension_vif = false;
                    VIFCombinable vc = toVIFCombinable(combinable_full_vif);
                    found_combinable_vifs.insert(vc);
                    found_combinable_vifs_raw.insert(combinable_full_vif);

                    if (data_has_difvifs)
                    {
                        t->addExplanationAndIncrementPos(*format, 1, KindOfData::PROTOCOL, Understanding::FULL,
                            "%02X combinable extension vife", vife);
                    }
                }
                else
                {
                    combinable_full_vif = vife & 0x7f;
                    // Check if this is marker for one of the extended combinable vifs
                    if (combinable_full_vif == 0x7c || combinable_full_vif == 0x7f)
                    {
                        combinable_full_vif <<= 8;
                        combinable_extension_vif = true;
                        VIFCombinable vc = toVIFCombinable(vife & 0x7f);
                        if (data_has_difvifs)
                        {
                            t->addExplanationAndIncrementPos(*format, 1, KindOfData::PROTOCOL, Understanding::FULL,
                                "%02X combinable vif (%s)", vife, toString(vc));
                        }
                    }
                    else
                    {
                        VIFCombinable vc = toVIFCombinable(combinable_full_vif);
                        found_combinable_vifs.insert(vc);
                        found_combinable_vifs_raw.insert(combinable_full_vif);

                        if (data_has_difvifs)
                        {
                            t->addExplanationAndIncrementPos(*format, 1, KindOfData::PROTOCOL, Understanding::FULL,
                                "%02X combinable vif (%s)", vife, toString(vc));
                        }
                    }
                }
            }
        }

        dv = "";
        for (uchar c : id_bytes) {
            char hex[3];
            hex[2] = 0;
            snprintf(hex, 3, "%02X", c);
            dv.append(hex);
        }
        DEBUG_PARSER("(dvparser debug) key \"%s\"", dv.c_str());

        int count = ++dv_count[dv];
        if (count > 1) {
            strprintf(&key, "%s_%d", dv.c_str(), count);
        }
        else {
            strprintf(&key, "%s", dv.c_str());
        }
        DEBUG_PARSER("(dvparser debug) DifVif key is %s", key.c_str());

        int remaining = std::distance(data, data_end);
        if (remaining < 1)
        {
            debug("(dvparser) warning: unexpected end of data");
            break;
        }

        if (variable_length) {
            DEBUG_PARSER("(dvparser debug) varlen %02x", *(data + 0));
            datalen = *(data);
            t->addExplanationAndIncrementPos(data, 1, KindOfData::PROTOCOL, Understanding::FULL, "%02X varlen=%d", *(data + 0), datalen);
            remaining--; // Drop the length byte.
        }
        DEBUG_PARSER("(dvparser debug) remaining data %d len=%d", remaining, datalen);
        if (remaining < datalen)
        {
            debug("(dvparser) warning: unexpected end of data");
            datalen = remaining - 1;
        }

        string value = bin2hex(data, data_end, datalen);
        int offset = start_parse_here + data - data_start;

        (*dv_entries)[key] = { offset, DVEntry(offset,
                                               key,
                                               mt,
                                               Vif(full_vif),
                                               found_combinable_vifs,
                                               found_combinable_vifs_raw,
                                               StorageNr(storage_nr),
                                               TariffNr(tariff),
                                               SubUnitNr(subunit),
                                               value) };

        DVEntry* dve = &(*dv_entries)[key].second;

        if (isTraceEnabled())
        {
            debug("[DVPARSER] entry %s", dve->str().c_str());
        }

        assert(key == dve->dif_vif_key.str());

        if (value.length() > 0) {
            // This call increments data with datalen.
            t->addExplanationAndIncrementPos(data, datalen, KindOfData::CONTENT, Understanding::NONE, "%s", value.c_str());
            DEBUG_PARSER("(dvparser debug) data \"%s\"", value.c_str());
        }
        if (remaining == datalen || data == databytes.end()) {
            // We are done here!
            break;
        }
    }

    string format_string = bin2hex(format_bytes);
    uint16_t hash = crc16_EN13757(safeButUnsafeVectorPtr(format_bytes), format_bytes.size());

    if (data_has_difvifs) {
        if (hash_to_format_.count(hash) == 0) {
            hash_to_format_[hash] = format_string;
            debug("(dvparser) found new format \"%s\" with hash %x, remembering!", format_string.c_str(), hash);
        }
    }

    return true;
}

bool hasKey(std::map<std::string, std::pair<int, DVEntry>>* dv_entries, std::string key)
{
    return dv_entries->count(key) > 0;
}

bool findKey(MeasurementType mit, VIFRange vif_range, StorageNr storagenr, TariffNr tariffnr,
    std::string* key, std::map<std::string, std::pair<int, DVEntry>>* dv_entries)
{
    return findKeyWithNr(mit, vif_range, storagenr, tariffnr, 1, key, dv_entries);
}

bool findKeyWithNr(MeasurementType mit, VIFRange vif_range, StorageNr storagenr, TariffNr tariffnr, int nr,
    std::string* key, std::map<std::string, std::pair<int, DVEntry>>* dv_entries)
{
    /*debug("(dvparser) looking for type=%s vifrange=%s storagenr=%d tariffnr=%d",
      measurementTypeName(mit).c_str(), toString(vif_range), storagenr.intValue(), tariffnr.intValue());*/

    for (auto& v : *dv_entries)
    {
        MeasurementType ty = v.second.second.measurement_type;
        Vif vi = v.second.second.vif;
        StorageNr sn = v.second.second.storage_nr;
        TariffNr tn = v.second.second.tariff_nr;

        /* debug("(dvparser) match? %s type=%s vife=%x (%s) and storagenr=%d",
              v.first.c_str(),
              measurementTypeName(ty).c_str(), vi.intValue(), storagenr, sn);*/

        if (isInsideVIFRange(vi, vif_range) &&
            (mit == MeasurementType::Instantaneous || mit == ty) &&
            (storagenr == AnyStorageNr || storagenr == sn) &&
            (tariffnr == AnyTariffNr || tariffnr == tn))
        {
            *key = v.first;
            nr--;
            if (nr <= 0) return true;
            debug("(dvparser) found key %s for type=%s vif=%x storagenr=%d",
                v.first.c_str(), measurementTypeName(ty).c_str(),
                vi.intValue(), storagenr.intValue());
        }
    }
    return false;
}

void extractDV(DifVifKey& dvk, uchar* dif, int* vif, bool* has_difes, bool* has_vifes)
{
    string tmp = dvk.str();
    extractDV(tmp, dif, vif, has_difes, has_vifes);
}

void extractDV(string& s, uchar* dif, int* vif, bool* has_difes, bool* has_vifes)
{
    vector<uchar> bytes;
    hex2bin(s, &bytes);
    size_t i = 0;
    *has_difes = false;
    *has_vifes = false;
    if (bytes.size() == 0)
    {
        *dif = 0;
        *vif = 0;
        return;
    }

    *dif = bytes[i];
    while (i < bytes.size() && (bytes[i] & 0x80))
    {
        i++;
        *has_difes = true;
    }
    i++;

    if (i >= bytes.size())
    {
        *vif = 0;
        return;
    }

    *vif = bytes[i];
    if (*vif == 0xfb || // first extension
        *vif == 0xfd || // second extensio
        *vif == 0xef || // third extension
        *vif == 0xff)   // vendor extension
    {
        if (i + 1 < bytes.size())
        {
            // Create an extended vif, like 0xfd31 for example.
            *vif = bytes[i] << 8 | bytes[i + 1];
            i++;
        }
    }

    while (i < bytes.size() && (bytes[i] & 0x80))
    {
        i++;
        *has_vifes = true;
    }
}

bool extractDVuint8( std::map<string, pair<int, DVEntry>>* dv_entries,
    string key,
    int* offset,
    uchar* value)
{
    if ((*dv_entries).count(key) == 0) {
        verbose("(dvparser) warning: cannot extract uint8 from non-existant key \"%s\"", key.c_str());
        *offset = -1;
        *value = 0;
        return false;
    }

    pair<int, DVEntry>& p = (*dv_entries)[key];
    *offset = p.first;
    vector<uchar> v;
    hex2bin(p.second.value, &v);

    *value = v[0];
    return true;
}

bool extractDVuint16( std::map<string, pair<int, DVEntry>>* dv_entries,
    string key,
    int* offset,
    uint16_t* value)
{
    if ((*dv_entries).count(key) == 0) {
        verbose("(dvparser) warning: cannot extract uint16 from non-existant key \"%s\"", key.c_str());
        *offset = -1;
        *value = 0;
        return false;
    }

    pair<int, DVEntry>& p = (*dv_entries)[key];
    *offset = p.first;
    vector<uchar> v;
    hex2bin(p.second.value, &v);

    *value = v[1] << 8 | v[0];
    return true;
}

bool extractDVuint24( std::map<string, pair<int, DVEntry>>* dv_entries,
    string key,
    int* offset,
    uint32_t* value)
{
    if ((*dv_entries).count(key) == 0) {
        verbose("(dvparser) warning: cannot extract uint24 from non-existant key \"%s\"", key.c_str());
        *offset = -1;
        *value = 0;
        return false;
    }

    pair<int, DVEntry>& p = (*dv_entries)[key];
    *offset = p.first;
    vector<uchar> v;
    hex2bin(p.second.value, &v);

    *value = v[2] << 16 | v[1] << 8 | v[0];
    return true;
}

bool extractDVuint32( std::map<string, pair<int, DVEntry>>* dv_entries,
    string key,
    int* offset,
    uint32_t* value)
{
    if ((*dv_entries).count(key) == 0) {
        verbose("(dvparser) warning: cannot extract uint32 from non-existant key \"%s\"", key.c_str());
        *offset = -1;
        *value = 0;
        return false;
    }

    pair<int, DVEntry>& p = (*dv_entries)[key];
    *offset = p.first;
    vector<uchar> v;
    hex2bin(p.second.value, &v);

    *value = (uint32_t(v[3]) << 24) | (uint32_t(v[2]) << 16) | (uint32_t(v[1]) << 8) | uint32_t(v[0]);
    return true;
}

bool extractDVdouble(std::map<string, pair<int, DVEntry>>* dv_entries,
    string key,
    int* offset,
    double* value,
    bool auto_scale,
    bool force_unsigned)
{
    if ((*dv_entries).count(key) == 0) {
        verbose("(dvparser) warning: cannot extract double from non-existant key \"%s\"", key.c_str());
        *offset = 0;
        *value = 0;
        return false;
    }
    pair<int, DVEntry>& p = (*dv_entries)[key];
    *offset = p.first;

    if (p.second.value.length() == 0) {
        verbose("(dvparser) warning: key found but no data  \"%s\"", key.c_str());
        *offset = 0;
        *value = 0;
        return false;
    }

    return p.second.extractDouble(value, auto_scale, force_unsigned);
}

bool checkSizeHex(size_t expected_len, DifVifKey& dvk, string& v)
{
    if (v.length() == expected_len) return true;

    warning("(dvparser) bad decode since difvif %s expected %d hex chars but got \"%s\"",
        dvk.str().c_str(), expected_len, v.c_str());
    return false;
}

bool is_all_F(string& v)
{
    for (size_t i = 0; i < v.length(); ++i)
    {
        if (v[i] != 'F') return false;
    }
    return true;
}


double vifScale(int vif)
{
    // Remove any remaining 0x80 top bits.
    vif &= 0x7f7f;

    switch (vif) {
        // wmbusmeters always returns enery as kwh
    case 0x00: return 1000000.0; // Energy mWh
    case 0x01: return 100000.0;  // Energy 10⁻² Wh
    case 0x02: return 10000.0;   // Energy 10⁻¹ Wh
    case 0x03: return 1000.0;    // Energy Wh
    case 0x04: return 100.0;     // Energy 10¹ Wh
    case 0x05: return 10.0;      // Energy 10² Wh
    case 0x06: return 1.0;       // Energy kWh
    case 0x07: return 0.1;       // Energy 10⁴ Wh

        // or wmbusmeters always returns energy as MJ
    case 0x08: return 1000000.0; // Energy J
    case 0x09: return 100000.0;  // Energy 10¹ J
    case 0x0A: return 10000.0;   // Energy 10² J
    case 0x0B: return 1000.0;    // Energy kJ
    case 0x0C: return 100.0;     // Energy 10⁴ J
    case 0x0D: return 10.0;      // Energy 10⁵ J
    case 0x0E: return 1.0;       // Energy MJ
    case 0x0F: return 0.1;       // Energy 10⁷ J

        // wmbusmeters always returns volume as m3
    case 0x10: return 1000000.0; // Volume cm³
    case 0x11: return 100000.0; // Volume 10⁻⁵ m³
    case 0x12: return 10000.0; // Volume 10⁻⁴ m³
    case 0x13: return 1000.0; // Volume l
    case 0x14: return 100.0; // Volume 10⁻² m³
    case 0x15: return 10.0; // Volume 10⁻¹ m³
    case 0x16: return 1.0; // Volume m³
    case 0x17: return 0.1; // Volume 10¹ m³

        // wmbusmeters always returns weight in kg
    case 0x18: return 1000.0; // Mass g
    case 0x19: return 100.0;  // Mass 10⁻² kg
    case 0x1A: return 10.0;   // Mass 10⁻¹ kg
    case 0x1B: return 1.0;    // Mass kg
    case 0x1C: return 0.1;    // Mass 10¹ kg
    case 0x1D: return 0.01;   // Mass 10² kg
    case 0x1E: return 0.001;  // Mass t
    case 0x1F: return 0.0001; // Mass 10⁴ kg

        // wmbusmeters always returns time in hours
    case 0x20: return 3600.0;     // On time seconds
    case 0x21: return 60.0;       // On time minutes
    case 0x22: return 1.0;        // On time hours
    case 0x23: return (1.0 / 24.0); // On time days

    case 0x24: return 3600.0;     // Operating time seconds
    case 0x25: return 60.0;       // Operating time minutes
    case 0x26: return 1.0;        // Operating time hours
    case 0x27: return (1.0 / 24.0); // Operating time days

        // wmbusmeters always returns power in kw
    case 0x28: return 1000000.0; // Power mW
    case 0x29: return 100000.0; // Power 10⁻² W
    case 0x2A: return 10000.0; // Power 10⁻¹ W
    case 0x2B: return 1000.0; // Power W
    case 0x2C: return 100.0; // Power 10¹ W
    case 0x2D: return 10.0; // Power 10² W
    case 0x2E: return 1.0; // Power kW
    case 0x2F: return 0.1; // Power 10⁴ W

        // or wmbusmeters always returns power in MJh
    case 0x30: return 1000000.0; // Power J/h
    case 0x31: return 100000.0; // Power 10¹ J/h
    case 0x32: return 10000.0; // Power 10² J/h
    case 0x33: return 1000.0; // Power kJ/h
    case 0x34: return 100.0; // Power 10⁴ J/h
    case 0x35: return 10.0; // Power 10⁵ J/h
    case 0x36: return 1.0; // Power MJ/h
    case 0x37: return 0.1; // Power 10⁷ J/h

        // wmbusmeters always returns volume flow in m3h
    case 0x38: return 1000000.0; // Volume flow cm³/h
    case 0x39: return 100000.0; // Volume flow 10⁻⁵ m³/h
    case 0x3A: return 10000.0; // Volume flow 10⁻⁴ m³/h
    case 0x3B: return 1000.0; // Volume flow l/h
    case 0x3C: return 100.0; // Volume flow 10⁻² m³/h
    case 0x3D: return 10.0; // Volume flow 10⁻¹ m³/h
    case 0x3E: return 1.0; // Volume flow m³/h
    case 0x3F: return 0.1; // Volume flow 10¹ m³/h

        // wmbusmeters always returns volume flow in m3h
    case 0x40: return 600000000.0; // Volume flow ext. 10⁻⁷ m³/min
    case 0x41: return 60000000.0; // Volume flow ext. cm³/min
    case 0x42: return 6000000.0; // Volume flow ext. 10⁻⁵ m³/min
    case 0x43: return 600000.0; // Volume flow ext. 10⁻⁴ m³/min
    case 0x44: return 60000.0; // Volume flow ext. l/min
    case 0x45: return 6000.0; // Volume flow ext. 10⁻² m³/min
    case 0x46: return 600.0; // Volume flow ext. 10⁻¹ m³/min
    case 0x47: return 60.0; // Volume flow ext. m³/min

        // this flow numbers will be small in the m3h unit, but it
        // does not matter since double stores the scale factor in its exponent.
    case 0x48: return 1000000000.0 * 3600; // Volume flow ext. mm³/s
    case 0x49: return 100000000.0 * 3600; // Volume flow ext. 10⁻⁸ m³/s
    case 0x4A: return 10000000.0 * 3600; // Volume flow ext. 10⁻⁷ m³/s
    case 0x4B: return 1000000.0 * 3600; // Volume flow ext. cm³/s
    case 0x4C: return 100000.0 * 3600; // Volume flow ext. 10⁻⁵ m³/s
    case 0x4D: return 10000.0 * 3600; // Volume flow ext. 10⁻⁴ m³/s
    case 0x4E: return 1000.0 * 3600; // Volume flow ext. l/s
    case 0x4F: return 100.0 * 3600; // Volume flow ext. 10⁻² m³/s

        // wmbusmeters always returns mass flow as kgh
    case 0x50: return 1000.0; // Mass g/h
    case 0x51: return 100.0; // Mass 10⁻² kg/h
    case 0x52: return 10.0; // Mass 10⁻¹ kg/h
    case 0x53: return 1.0; // Mass kg/h
    case 0x54: return 0.1; // Mass 10¹ kg/h
    case 0x55: return 0.01; // Mass 10² kg/h
    case 0x56: return 0.001; // Mass t/h
    case 0x57: return 0.0001; // Mass 10⁴ kg/h

        // wmbusmeters always returns temperature in °C
    case 0x58: return 1000.0; // Flow temperature 10⁻³ °C
    case 0x59: return 100.0; // Flow temperature 10⁻² °C
    case 0x5A: return 10.0; // Flow temperature 10⁻¹ °C
    case 0x5B: return 1.0; // Flow temperature °C

        // wmbusmeters always returns temperature in c
    case 0x5C: return 1000.0;  // Return temperature 10⁻³ °C
    case 0x5D: return 100.0; // Return temperature 10⁻² °C
    case 0x5E: return 10.0; // Return temperature 10⁻¹ °C
    case 0x5F: return 1.0; // Return temperature °C

        // or if Kelvin is used as a temperature, in K
        // what kind of meter cares about -273.15 °C
        // a flow pump for liquid helium perhaps?
    case 0x60: return 1000.0; // Temperature difference mK
    case 0x61: return 100.0; // Temperature difference 10⁻² K
    case 0x62: return 10.0; // Temperature difference 10⁻¹ K
    case 0x63: return 1.0; // Temperature difference K

        // wmbusmeters always returns temperature in c
    case 0x64: return 1000.0; // External temperature 10⁻³ °C
    case 0x65: return 100.0; // External temperature 10⁻² °C
    case 0x66: return 10.0; // External temperature 10⁻¹ °C
    case 0x67: return 1.0; // External temperature °C

        // wmbusmeters always returns pressure in bar
    case 0x68: return 1000.0; // Pressure mbar
    case 0x69: return 100.0; // Pressure 10⁻² bar
    case 0x6A: return 10.0; // Pressure 10⁻¹ bar
    case 0x6B: return 1.0; // Pressure bar

    case 0x6C: return 1.0; // Date type G
    case 0x6D: return 1.0; // Date&Time type F
    case 0x6E: return 1.0; // Units for H.C.A. are never scaled
    case 0x6F: warning("(wmbus) warning: do not scale a reserved type!"); return -1.0; // Reserved

        // wmbusmeters always returns time in hours
    case 0x70: return 3600.0; // Averaging duration seconds
    case 0x71: return 60.0; // Averaging duration minutes
    case 0x72: return 1.0; // Averaging duration hours
    case 0x73: return (1.0 / 24.0); // Averaging duration days

        // wmbusmeters always returns time in hours
    case 0x74: return 3600.0; // Actuality duration seconds
    case 0x75: return 60.0; // Actuality duration minutes
    case 0x76: return 1.0; // Actuality duration hours
    case 0x77: return (1.0 / 24.0); // Actuality duration days

        // Active energy 0.1 or 1 MWh normalize to 100 KWh or 1000 KWh
        // 7b00 33632 -> 3363.2 MWh -> 3363200 KWh
        // 7b01 33632 -> 33632 MWh -> 33632000 KWh
    case 0x7b00:
    case 0x7b01: { double exp = (vif & 0x1) + 2; return pow(10.0, -exp); }

               // relative humidity is a dimensionless value.
    case 0x7b1a: return 10.0; // Relative humidity 0.1 %
    case 0x7b1b: return 1.0;  // Relative humidity 1 %

        // wmbusmeters always returns time in hours
        // 0x7d30 is not supposed to be used according to spec.
    case 0x7d31: return 60.0; // Duration tariff minutes
    case 0x7d32: return 1.0; // Duration tariff hours
    case 0x7d33: return (1.0 / 24.0); // Duration tariff days

        // wmbusmeters always voltage in volts
    case 0x7d40:
    case 0x7d41:
    case 0x7d42:
    case 0x7d43:
    case 0x7d44:
    case 0x7d45:
    case 0x7d46:
    case 0x7d47:
    case 0x7d48:
    case 0x7d49:
    case 0x7d4a:
    case 0x7d4b:
    case 0x7d4c:
    case 0x7d4d:
    case 0x7d4e:
    case 0x7d4f: { double exp = (vif & 0xf) - 9; return pow(10.0, -exp); }

               // wmbusmeters always return current in ampere
    case 0x7d50:
    case 0x7d51:
    case 0x7d52:
    case 0x7d53:
    case 0x7d54:
    case 0x7d55:
    case 0x7d56:
    case 0x7d57:
    case 0x7d58:
    case 0x7d59:
    case 0x7d5a:
    case 0x7d5b:
    case 0x7d5c:
    case 0x7d5d:
    case 0x7d5e:
    case 0x7d5f: { double exp = (vif & 0xf) - 12; return pow(10.0, -exp); }

               // for remaining battery wmbusmeters returns number of days.
    case 0x7d74: { return 1.0; }

               // wmbusmeters always returns time in hours
    case 0x7d2c: return 3600.0; // Duration since readout seconds
    case 0x7d2d: return 60.0; // Duration since readout minutes
    case 0x7d2e: return 1.0; // Duration since readout hours
    case 0x7d2f: return (1.0 / 24.0); // Duration since readout days

        /*
    case 0x78: // Fabrication no
    case 0x79: // Enhanced identification
    case 0x80: // Address

    case 0x7C: // VIF in following string (length in first byte)
    case 0x7E: // Any VIF
    case 0x7F: // Manufacturer specific
        */

    default: warning("(wmbus) warning: type 0x%x cannot be scaled!", vif);
        return -1;
    }
}

bool DVEntry::extractDouble(double* out, bool auto_scale, bool force_unsigned)
{
    int t = dif_vif_key.dif() & 0xf;
    if (t == 0x0 ||
        t == 0x8 ||
        t == 0xd ||
        t == 0xf)
    {
        // Cannot extract from nothing, selection for readout, variable length or special.
        // Variable length is used for compact varlen history. Should be added in the future.
        return false;
    }
    else
        if (t == 0x1 || // 8 Bit Integer/Binary
            t == 0x2 || // 16 Bit Integer/Binary
            t == 0x3 || // 24 Bit Integer/Binary
            t == 0x4 || // 32 Bit Integer/Binary
            t == 0x6 || // 48 Bit Integer/Binary
            t == 0x7)   // 64 Bit Integer/Binary
        {
            vector<uchar> v;
            hex2bin(value, &v);
            uint64_t raw = 0;
            bool negate = false;
            uint64_t negate_mask = 0;
            if (t == 0x1) {
                if (!checkSizeHex(2, dif_vif_key, value)) return false;
                assert(v.size() == 1);
                raw = v[0];
                if (!force_unsigned && (raw & (uint64_t)0x80UL) != 0) { negate = true; negate_mask = ~((uint64_t)0) << 8; }
            }
            else if (t == 0x2) {
                if (!checkSizeHex(4, dif_vif_key, value)) return false;
                assert(v.size() == 2);
                raw = v[1] * 256 + v[0];
                if (!force_unsigned && (raw & (uint64_t)0x8000UL) != 0) { negate = true; negate_mask = ~((uint64_t)0) << 16; }
            }
            else if (t == 0x3) {
                if (!checkSizeHex(6, dif_vif_key, value)) return false;
                assert(v.size() == 3);
                raw = v[2] * 256 * 256 + v[1] * 256 + v[0];
                if (!force_unsigned && (raw & (uint64_t)0x800000UL) != 0) { negate = true; negate_mask = ~((uint64_t)0) << 24; }
            }
            else if (t == 0x4) {
                if (!checkSizeHex(8, dif_vif_key, value)) return false;
                assert(v.size() == 4);
                raw = ((unsigned int)v[3]) * 256 * 256 * 256
                    + ((unsigned int)v[2]) * 256 * 256
                    + ((unsigned int)v[1]) * 256
                    + ((unsigned int)v[0]);
                if (!force_unsigned && (raw & (uint64_t)0x80000000UL) != 0) { negate = true; negate_mask = ~((uint64_t)0) << 32; }
            }
            else if (t == 0x6) {
                if (!checkSizeHex(12, dif_vif_key, value)) return false;
                assert(v.size() == 6);
                raw = ((uint64_t)v[5]) * 256 * 256 * 256 * 256 * 256
                    + ((uint64_t)v[4]) * 256 * 256 * 256 * 256
                    + ((uint64_t)v[3]) * 256 * 256 * 256
                    + ((uint64_t)v[2]) * 256 * 256
                    + ((uint64_t)v[1]) * 256
                    + ((uint64_t)v[0]);
                if (!force_unsigned && (raw & (uint64_t)0x800000000000UL) != 0) { negate = true; negate_mask = ~((uint64_t)0) << 48; }
            }
            else if (t == 0x7) {
                if (!checkSizeHex(16, dif_vif_key, value)) return false;
                assert(v.size() == 8);
                raw = ((uint64_t)v[7]) * 256 * 256 * 256 * 256 * 256 * 256 * 256
                    + ((uint64_t)v[6]) * 256 * 256 * 256 * 256 * 256 * 256
                    + ((uint64_t)v[5]) * 256 * 256 * 256 * 256 * 256
                    + ((uint64_t)v[4]) * 256 * 256 * 256 * 256
                    + ((uint64_t)v[3]) * 256 * 256 * 256
                    + ((uint64_t)v[2]) * 256 * 256
                    + ((uint64_t)v[1]) * 256
                    + ((uint64_t)v[0]);
                if (!force_unsigned && (raw & (uint64_t)0x8000000000000000UL) != 0) { negate = true; negate_mask = 0; }
            }
            double scale = 1.0;
            double draw = (double)raw;
            if (negate)
            {
                draw = (double)((int64_t)(negate_mask | raw));
            }
            if (auto_scale) scale = vifScale(dif_vif_key.vif());
            *out = (draw) / scale;
        }
        else
            if (t == 0x9 || // 2 digit BCD
                t == 0xA || // 4 digit BCD
                t == 0xB || // 6 digit BCD
                t == 0xC || // 8 digit BCD
                t == 0xE)   // 12 digit BCD
            {
                // Negative BCD values are always visible in bcd. I.e. they are always signed.
                // Ignore assumption on signedness.
                // 74140000 -> 00001474
                string& v = value;
                uint64_t raw = 0;
                bool negate = false;

                if (is_all_F(v))
                {
                    *out = std::nan("");
                    return false;
                }
                if (t == 0x9) {
                    if (!checkSizeHex(2, dif_vif_key, v)) return false;
                    if (v[0] == 'F') { negate = true; v[0] = '0'; }
                    raw = (v[0] - '0') * 10 + (v[1] - '0');
                }
                else if (t == 0xA) {
                    if (!checkSizeHex(4, dif_vif_key, v)) return false;
                    if (v[2] == 'F') { negate = true; v[2] = '0'; }
                    raw = (v[2] - '0') * 10 * 10 * 10 + (v[3] - '0') * 10 * 10
                        + (v[0] - '0') * 10 + (v[1] - '0');
                }
                else if (t == 0xB) {
                    if (!checkSizeHex(6, dif_vif_key, v)) return false;
                    if (v[4] == 'F') { negate = true; v[4] = '0'; }
                    raw = (v[4] - '0') * 10 * 10 * 10 * 10 * 10 + (v[5] - '0') * 10 * 10 * 10 * 10
                        + (v[2] - '0') * 10 * 10 * 10 + (v[3] - '0') * 10 * 10
                        + (v[0] - '0') * 10 + (v[1] - '0');
                }
                else if (t == 0xC) {
                    if (!checkSizeHex(8, dif_vif_key, v)) return false;
                    if (v[6] == 'F') { negate = true; v[6] = '0'; }
                    raw = (v[6] - '0') * 10 * 10 * 10 * 10 * 10 * 10 * 10 + (v[7] - '0') * 10 * 10 * 10 * 10 * 10 * 10
                        + (v[4] - '0') * 10 * 10 * 10 * 10 * 10 + (v[5] - '0') * 10 * 10 * 10 * 10
                        + (v[2] - '0') * 10 * 10 * 10 + (v[3] - '0') * 10 * 10
                        + (v[0] - '0') * 10 + (v[1] - '0');
                }
                else if (t == 0xE) {
                    if (!checkSizeHex(12, dif_vif_key, v)) return false;
                    if (v[10] == 'F') { negate = true; v[10] = '0'; }
                    raw = (v[10] - '0') * 10 * 10 * 10 * 10 * 10 * 10 * 10 * 10 * 10 * 10 * 10 + (v[11] - '0') * 10 * 10 * 10 * 10 * 10 * 10 * 10 * 10 * 10 * 10
                        + (v[8] - '0') * 10 * 10 * 10 * 10 * 10 * 10 * 10 * 10 * 10 + (v[9] - '0') * 10 * 10 * 10 * 10 * 10 * 10 * 10 * 10
                        + (v[6] - '0') * 10 * 10 * 10 * 10 * 10 * 10 * 10 + (v[7] - '0') * 10 * 10 * 10 * 10 * 10 * 10
                        + (v[4] - '0') * 10 * 10 * 10 * 10 * 10 + (v[5] - '0') * 10 * 10 * 10 * 10
                        + (v[2] - '0') * 10 * 10 * 10 + (v[3] - '0') * 10 * 10
                        + (v[0] - '0') * 10 + (v[1] - '0');
                }
                double scale = 1.0;
                double draw = (double)raw;
                if (negate)
                {
                    draw = (double)draw * -1;
                }
                if (auto_scale) scale = vifScale(dif_vif_key.vif());
                *out = (draw) / scale;
            }
            else
                if (t == 0x5) // 32 Bit Real
                {
                    vector<uchar> v;
                    hex2bin(value, &v);
                    if (!checkSizeHex(8, dif_vif_key, value)) return false;
                    assert(v.size() == 4);
                    RealConversion rc;
                    rc.i = v[3] << 24 | v[2] << 16 | v[1] << 8 | v[0];

                    // Assumes float uses the standard IEEE 754 bit set.
                    // 1 bit sign,  8 bit exp, 23 bit mantissa
                    // RealConversion is tested on an amd64 platform. How about
                    // other platsforms with different byte ordering?
                    double draw = rc.f;
                    double scale = 1.0;
                    if (auto_scale) scale = vifScale(dif_vif_key.vif());
                    *out = (draw) / scale;
                }
                else
                {
                    warning("(dvparser) Unsupported dif format for extraction to double! dif=%02x", dif_vif_key.dif());
                    return false;
                }

    return true;
}

bool extractDVlong( std::map<string, pair<int, DVEntry>>* dv_entries,
    string key,
    int* offset,
    uint64_t* out)
{
    if ((*dv_entries).count(key) == 0) {
        verbose("(dvparser) warning: cannot extract long from non-existant key \"%s\"", key.c_str());
        *offset = 0;
        *out = 0;
        return false;
    }

    pair<int, DVEntry>& p = (*dv_entries)[key];
    *offset = p.first;

    if (p.second.value.length() == 0) {
        verbose("(dvparser) warning: key found but no data  \"%s\"", key.c_str());
        *offset = 0;
        *out = 0;
        return false;
    }

    return p.second.extractLong(out);
}

bool DVEntry::extractLong(uint64_t* out)
{
    int t = dif_vif_key.dif() & 0xf;
    if (t == 0x1 || // 8 Bit Integer/Binary
        t == 0x2 || // 16 Bit Integer/Binary
        t == 0x3 || // 24 Bit Integer/Binary
        t == 0x4 || // 32 Bit Integer/Binary
        t == 0x6 || // 48 Bit Integer/Binary
        t == 0x7)   // 64 Bit Integer/Binary
    {
        vector<uchar> v;
        hex2bin(value, &v);
        uint64_t raw = 0;
        if (t == 0x1) {
            if (!checkSizeHex(2, dif_vif_key, value)) return false;
            assert(v.size() == 1);
            raw = v[0];
        }
        else if (t == 0x2) {
            if (!checkSizeHex(4, dif_vif_key, value)) return false;
            assert(v.size() == 2);
            raw = v[1] * 256 + v[0];
        }
        else if (t == 0x3) {
            if (!checkSizeHex(6, dif_vif_key, value)) return false;
            assert(v.size() == 3);
            raw = v[2] * 256 * 256 + v[1] * 256 + v[0];
        }
        else if (t == 0x4) {
            if (!checkSizeHex(8, dif_vif_key, value)) return false;
            assert(v.size() == 4);
            raw = ((unsigned int)v[3]) * 256 * 256 * 256
                + ((unsigned int)v[2]) * 256 * 256
                + ((unsigned int)v[1]) * 256
                + ((unsigned int)v[0]);
        }
        else if (t == 0x6) {
            if (!checkSizeHex(12, dif_vif_key, value)) return false;
            assert(v.size() == 6);
            raw = ((uint64_t)v[5]) * 256 * 256 * 256 * 256 * 256
                + ((uint64_t)v[4]) * 256 * 256 * 256 * 256
                + ((uint64_t)v[3]) * 256 * 256 * 256
                + ((uint64_t)v[2]) * 256 * 256
                + ((uint64_t)v[1]) * 256
                + ((uint64_t)v[0]);
        }
        else if (t == 0x7) {
            if (!checkSizeHex(16, dif_vif_key, value)) return false;
            assert(v.size() == 8);
            raw = ((uint64_t)v[7]) * 256 * 256 * 256 * 256 * 256 * 256 * 256
                + ((uint64_t)v[6]) * 256 * 256 * 256 * 256 * 256 * 256
                + ((uint64_t)v[5]) * 256 * 256 * 256 * 256 * 256
                + ((uint64_t)v[4]) * 256 * 256 * 256 * 256
                + ((uint64_t)v[3]) * 256 * 256 * 256
                + ((uint64_t)v[2]) * 256 * 256
                + ((uint64_t)v[1]) * 256
                + ((uint64_t)v[0]);
        }
        *out = raw;
    }
    else
        if (t == 0x9 || // 2 digit BCD
            t == 0xA || // 4 digit BCD
            t == 0xB || // 6 digit BCD
            t == 0xC || // 8 digit BCD
            t == 0xE)   // 12 digit BCD
        {
            // 74140000 -> 00001474
            string& v = value;
            if (is_all_F(v))
            {
                return false;
            }
            uint64_t raw = 0;
            bool negate = false;
            if (t == 0x9) {
                if (!checkSizeHex(2, dif_vif_key, value)) return false;
                if (v[0] == 'F') { negate = true; v[0] = '0'; }
                assert(v.size() == 2);
                raw = (v[0] - '0') * 10 + (v[1] - '0');
            }
            else if (t == 0xA) {
                if (!checkSizeHex(4, dif_vif_key, value)) return false;
                if (v[2] == 'F') { negate = true; v[2] = '0'; }
                assert(v.size() == 4);
                raw = (v[2] - '0') * 10 * 10 * 10 + (v[3] - '0') * 10 * 10
                    + (v[0] - '0') * 10 + (v[1] - '0');
            }
            else if (t == 0xB) {
                if (!checkSizeHex(6, dif_vif_key, value)) return false;
                if (v[4] == 'F') { negate = true; v[4] = '0'; }
                assert(v.size() == 6);
                raw = (v[4] - '0') * 10 * 10 * 10 * 10 * 10 + (v[5] - '0') * 10 * 10 * 10 * 10
                    + (v[2] - '0') * 10 * 10 * 10 + (v[3] - '0') * 10 * 10
                    + (v[0] - '0') * 10 + (v[1] - '0');
            }
            else if (t == 0xC) {
                if (!checkSizeHex(8, dif_vif_key, value)) return false;
                if (v[6] == 'F') { negate = true; v[6] = '0'; }
                assert(v.size() == 8);
                raw = (v[6] - '0') * 10 * 10 * 10 * 10 * 10 * 10 * 10 + (v[7] - '0') * 10 * 10 * 10 * 10 * 10 * 10
                    + (v[4] - '0') * 10 * 10 * 10 * 10 * 10 + (v[5] - '0') * 10 * 10 * 10 * 10
                    + (v[2] - '0') * 10 * 10 * 10 + (v[3] - '0') * 10 * 10
                    + (v[0] - '0') * 10 + (v[1] - '0');
            }
            else if (t == 0xE) {
                if (!checkSizeHex(12, dif_vif_key, value)) return false;
                if (v[10] == 'F') { negate = true; v[10] = '0'; }
                assert(v.size() == 12);
                raw = (v[10] - '0') * 10 * 10 * 10 * 10 * 10 * 10 * 10 * 10 * 10 * 10 * 10 + (v[11] - '0') * 10 * 10 * 10 * 10 * 10 * 10 * 10 * 10 * 10 * 10
                    + (v[8] - '0') * 10 * 10 * 10 * 10 * 10 * 10 * 10 * 10 * 10 + (v[9] - '0') * 10 * 10 * 10 * 10 * 10 * 10 * 10 * 10
                    + (v[6] - '0') * 10 * 10 * 10 * 10 * 10 * 10 * 10 + (v[7] - '0') * 10 * 10 * 10 * 10 * 10 * 10
                    + (v[4] - '0') * 10 * 10 * 10 * 10 * 10 + (v[5] - '0') * 10 * 10 * 10 * 10
                    + (v[2] - '0') * 10 * 10 * 10 + (v[3] - '0') * 10 * 10
                    + (v[0] - '0') * 10 + (v[1] - '0');
            }

            if (negate)
            {
                raw = (uint64_t)(((int64_t)raw) * -1);
            }

            *out = raw;
        }
        else
        {
            error("Unsupported dif format for extraction to long! dif=%02x", dif_vif_key.dif());
        }

    return true;
}

bool extractDVHexString( std::map<string, pair<int, DVEntry>>* dv_entries,
    string key,
    int* offset,
    string* value)
{
    if ((*dv_entries).count(key) == 0) {
        verbose("(dvparser) warning: cannot extract string from non-existant key \"%s\"", key.c_str());
        *offset = -1;
        return false;
    }
    pair<int, DVEntry>& p = (*dv_entries)[key];
    *offset = p.first;
    *value = p.second.value;

    return true;
}


bool extractDVReadableString( std::map<string, pair<int, DVEntry>>* dv_entries,
    string key,
    int* offset,
    string* out)
{
    if ((*dv_entries).count(key) == 0) {
        verbose("(dvparser) warning: cannot extract string from non-existant key \"%s\"", key.c_str());
        *offset = -1;
        return false;
    }
    pair<int, DVEntry>& p = (*dv_entries)[key];
    *offset = p.first;

    return p.second.extractReadableString(out);
}

bool DVEntry::extractReadableString(string* out)
{
    int t = dif_vif_key.dif() & 0xf;

    string v = value;

    if (t == 0x1 || // 8 Bit Integer/Binary
        t == 0x2 || // 16 Bit Integer/Binary
        t == 0x3 || // 24 Bit Integer/Binary
        t == 0x4 || // 32 Bit Integer/Binary
        t == 0x6 || // 48 Bit Integer/Binary
        t == 0x7 || // 64 Bit Integer/Binary
        t == 0xD)   // Variable length
    {
        if (isLikelyAscii(v))
        {
            // For example an enhanced id 32 bits binary looks like:
            // 44434241 and will be reversed to: 41424344 and translated using ascii
            // to ABCD
            v = reverseBinaryAsciiSafeToString(v);
        }
        else
        {
            v = reverseBCD(v);
        }
    }
    if (t == 0x9 || // 2 digit BCD
        t == 0xA || // 4 digit BCD
        t == 0xB || // 6 digit BCD
        t == 0xC || // 8 digit BCD
        t == 0xE)   // 12 digit BCD
    {
        // For example an enhanced id 12 digit bcd looks like:
        // 618171183100 and will be reversed to: 003118718161
        v = reverseBCD(v);
    }

    *out = v;
    return true;
}

double DVEntry::getCounter(DVEntryCounterType ct)
{
    switch (ct)
    {
    case DVEntryCounterType::STORAGE_COUNTER: return storage_nr.intValue();
    case DVEntryCounterType::TARIFF_COUNTER: return tariff_nr.intValue();
    case DVEntryCounterType::SUBUNIT_COUNTER: return subunit_nr.intValue();
    case DVEntryCounterType::UNKNOWN: break;
    }

    return std::numeric_limits<double>::quiet_NaN();
}

string DVEntry::str()
{
    string s =
        tostrprintf("%d: %s %s vif=%x %s%s st=%d ta=%d su=%d",
            offset,
            dif_vif_key.str().c_str(),
            toString(measurement_type),
            vif.intValue(),
            combinable_vifs.size() > 0 ? "HASCOMB " : "",
            combinable_vifs_raw.size() > 0 ? "HASCOMBRAW " : "",
            storage_nr.intValue(),
            tariff_nr.intValue(),
            subunit_nr.intValue()
        );

    return s;
}

bool extractDate(uchar hi, uchar lo, struct tm* date)
{
    // |     hi    |    lo     |
    // | YYYY MMMM | YYY DDDDD |

    int day = (0x1f) & lo;
    int year1 = ((0xe0) & lo) >> 5;
    int month = (0x0f) & hi;
    int year2 = ((0xf0) & hi) >> 1;
    int year = (2000 + year1 + year2);

    date->tm_mday = day;         /* Day of the month (1-31) */
    date->tm_mon = month - 1;    /* Month (0-11) */
    date->tm_year = year - 1900; /* Year - 1900 */

    if (month > 12) return false;
    return true;
}

bool extractTime(uchar hi, uchar lo, struct tm* date)
{
    // |    hi    |    lo    |
    // | ...hhhhh | ..mmmmmm |
    int min = (0x3f) & lo;
    int hour = (0x1f) & hi;

    date->tm_min = min;
    date->tm_hour = hour;

    if (min > 59) return false;
    if (hour > 23) return false;
    return true;
}

bool extractDVdate( std::map<string, pair<int, DVEntry>>* dv_entries,
    string key,
    int* offset,
    struct tm* out)
{
    if ((*dv_entries).count(key) == 0)
    {
        verbose("(dvparser) warning: cannot extract date from non-existant key \"%s\"", key.c_str());
        *offset = -1;
        memset(out, 0, sizeof(struct tm));
        return false;
    }
    pair<int, DVEntry>& p = (*dv_entries)[key];
    *offset = p.first;

    return p.second.extractDate(out);
}

bool DVEntry::extractDate(struct tm* out)
{
    memset(out, 0, sizeof(*out));
    out->tm_isdst = -1; // Figure out the dst automatically!

    vector<uchar> v;
    hex2bin(value, &v);

    bool ok = true;
    if (v.size() == 2) {
        ok &= ::extractDate(v[1], v[0], out);
    }
    else if (v.size() == 4) {
        ok &= ::extractDate(v[3], v[2], out);
        ok &= ::extractTime(v[1], v[0], out);
    }
    else if (v.size() == 6) {
        ok &= ::extractDate(v[4], v[3], out);
        ok &= ::extractTime(v[2], v[1], out);
        // ..ss ssss
        int sec = (0x3f) & v[0];
        out->tm_sec = sec;
        // There are also bits for day of week, week of year.
        // A bit for if daylight saving is in use or not and its offset.
        // A bit if it is a leap year.
        // I am unsure how to deal with this here..... TODO
    }

    return ok;
}

bool FieldMatcher::matches(DVEntry& dv_entry)
{
    if (!active) return false;

    // Test an explicit dif vif key.
    if (match_dif_vif_key)
    {
        bool b = dv_entry.dif_vif_key == dif_vif_key;
        return b;
    }

    // Test ranges and types.
    bool b =
        (!match_vif_range || isInsideVIFRange(dv_entry.vif, vif_range)) &&
        (!match_vif_raw || dv_entry.vif == vif_raw) &&
        (!match_measurement_type || dv_entry.measurement_type == measurement_type) &&
        (!match_storage_nr || (dv_entry.storage_nr >= storage_nr_from && dv_entry.storage_nr <= storage_nr_to)) &&
        (!match_tariff_nr || (dv_entry.tariff_nr >= tariff_nr_from && dv_entry.tariff_nr <= tariff_nr_to)) &&
        (!match_subunit_nr || (dv_entry.subunit_nr >= subunit_nr_from && dv_entry.subunit_nr <= subunit_nr_to));

    if (!b) return false;

    // So far so good, now test the combinables.

    // If field matcher has no combinables, then do NOT match any dventry with a combinable!
    if (vif_combinables.size() == 0 && vif_combinables_raw.size() == 0)
    {
        // If there is a combinable vif, then there is a raw combinable vif. So comparing both not strictly necessary.
        if (dv_entry.combinable_vifs.size() == 0 && dv_entry.combinable_vifs_raw.size() == 0) return true;
        // Oups, field matcher does not expect any combinables, but the dv_entry has combinables.
        // This means no match for us since combinables must be handled explicitly.
        return false;
    }

    // Lets check that the dv_entry vif combinables raw contains the field matcher requested combinable raws.
    // The raws are used for meters using reserved and manufacturer specific vif combinables.
    for (uint16_t vcr : vif_combinables_raw)
    {
        if (dv_entry.combinable_vifs_raw.count(vcr) == 0)
        {
            // Ouch, one of the requested vif combinables raw did not exist in the dv_entry. No match!
            return false;
        }
    }

    // Lets check that the dv_entry combinables contains the field matcher requested combinables.
    // The named vif combinables are used by well behaved meters.
    for (VIFCombinable vc : vif_combinables)
    {
        if (vc != VIFCombinable::Any && dv_entry.combinable_vifs.count(vc) == 0)
        {
            // Ouch, one of the requested combinables did not exist in the dv_entry. No match!
            return false;
        }
    }

    // Now if we have not selected the Any combinable match pattern,
    // then we need to check if there are unmatched combinables in the telegram, if so fail the match.
    if (vif_combinables.count(VIFCombinable::Any) == 0)
    {
        if (vif_combinables.size() > 0)
        {
            for (VIFCombinable vc : dv_entry.combinable_vifs)
            {
                if (vif_combinables.count(vc) == 0)
                {
                    // Oups, the telegram entry had a vif combinable that we had no matcher for.
                    return false;
                }
            }
        }
        else
        {
            for (uint16_t vcr : dv_entry.combinable_vifs_raw)
            {
                if (vif_combinables_raw.count(vcr) == 0)
                {
                    // Oups, the telegram entry had a vif combinable raw that we had no matcher for.
                    return false;
                }
            }
        }
    }

    // Yay, they were all found.
    return true;
}

const char* toString(MeasurementType mt)
{
    switch (mt)
    {
    case MeasurementType::Any: return "Any";
    case MeasurementType::Instantaneous: return "Instantaneous";
    case MeasurementType::Minimum: return "Minimum";
    case MeasurementType::Maximum: return "Maximum";
    case MeasurementType::AtError: return "AtError";
    case MeasurementType::Unknown: return "Unknown";
    }
    return "?";
}

MeasurementType toMeasurementType(const char* s)
{
    if (!strcmp(s, "Any")) return MeasurementType::Any;
    if (!strcmp(s, "Instantaneous")) return MeasurementType::Instantaneous;
    if (!strcmp(s, "Minimum")) return MeasurementType::Minimum;
    if (!strcmp(s, "Maximum")) return MeasurementType::Maximum;
    if (!strcmp(s, "AtError")) return MeasurementType::AtError;
    if (!strcmp(s, "Unknown")) return MeasurementType::Unknown;

    return MeasurementType::Unknown;
}

string FieldMatcher::str()
{
    string s = "";

    if (match_dif_vif_key)
    {
        s = s + "DVK(" + dif_vif_key.str() + ") ";
    }

    if (match_measurement_type)
    {
        s = s + "MT(" + toString(measurement_type) + ") ";
    }

    if (match_vif_range)
    {
        s = s + "VR(" + toString(vif_range) + ") ";
    }

    if (match_vif_raw)
    {
        s = s + "VRR(" + std::to_string(vif_raw) + ") ";
    }

    if (vif_combinables.size() > 0)
    {
        s += "Comb(";

        for (auto vc : vif_combinables)
        {
            s = s + toString(vc) + " ";
        }

        s.pop_back();
        s += ")";
    }

    if (match_storage_nr)
    {
        s = s + "S(" + std::to_string(storage_nr_from.intValue()) + "-" + std::to_string(storage_nr_to.intValue()) + ") ";
    }

    if (match_tariff_nr)
    {
        s = s + "T(" + std::to_string(tariff_nr_from.intValue()) + "-" + std::to_string(tariff_nr_to.intValue()) + ") ";
    }

    if (match_subunit_nr)
    {
        s += "U(" + std::to_string(subunit_nr_from.intValue()) + "-" + std::to_string(subunit_nr_to.intValue()) + ") ";
    }

    if (index_nr.intValue() != 1)
    {
        s += "I(" + std::to_string(index_nr.intValue()) + ")";
    }

    if (s.size() > 0)
    {
        s.pop_back();
    }

    return s;
}

DVEntryCounterType toDVEntryCounterType(const std::string& s)
{
    if (s == "storage_counter") return DVEntryCounterType::STORAGE_COUNTER;
    if (s == "tariff_counter") return DVEntryCounterType::TARIFF_COUNTER;
    if (s == "subunit_counter") return DVEntryCounterType::SUBUNIT_COUNTER;
    return DVEntryCounterType::UNKNOWN;
}

const char* toString(DVEntryCounterType ct)
{
    switch (ct)
    {
    case DVEntryCounterType::UNKNOWN: return "unknown";
    case DVEntryCounterType::STORAGE_COUNTER: return "storage_counter";
    case DVEntryCounterType::TARIFF_COUNTER: return "tariff_counter";
    case DVEntryCounterType::SUBUNIT_COUNTER: return "subunit_counter";
    }

    return "unknown";
}

string available_vif_ranges_;

const string& availableVIFRanges()
{
    if (available_vif_ranges_ != "") return available_vif_ranges_;

#define X(n,from,to,q,u) available_vif_ranges_ += string(#n) + "\n";
    LIST_OF_VIF_RANGES
#undef X

        // Remove last newline
        available_vif_ranges_.pop_back();
    return available_vif_ranges_;
}
