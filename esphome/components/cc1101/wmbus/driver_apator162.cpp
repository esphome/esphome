/*
 Copyright (C) 2017-2022 Fredrik Öhrström (gpl-3.0-or-later)

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

#include"meters_common_implementation.h"

namespace
{
    struct Driver : public virtual MeterCommonImplementation
    {
        Driver(MeterInfo &mi, DriverInfo &di);

        void processContent(Telegram *t);
        void processExtras(string miExtras);
        int registerSize(int c);
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("apator162");
        di.setDefaultFields("name,id,total_m3,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::T1);
        di.addLinkMode(LinkMode::C1);
        di.addDetection(MANUFACTURER_APA,  0x06,  0x05);
        di.addDetection(MANUFACTURER_APA,  0x07,  0x05);
        di.addDetection(0x8614 /*APT?*/, 0x07,  0x05); // Older version of telegram that is not understood!
        di.usesProcessContent();
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        processExtras(mi.extras);

        addNumericField(
            "total",
            Quantity::Volume,
            DEFAULT_PRINT_PROPERTIES,
            "The total water consumption recorded by this meter.");
    }

    void Driver::processContent(Telegram *t)
    {
        // Unfortunately, the at-wmbus-16-2 is mostly a proprietary protocol
        // simply wrapped inside a wmbus telegram.

        if (t->tpl_ci == 0xb6)
        {
            // Oups really old style telegram that we cannot decode.
            t->discard = true;
            return;
        }

        // Anyway, it seems like telegram is broken up into registers.
        // Each register is identified with a single byte after which the content follows.
        // For example, the total volume is marked by 0x10 followed by 4 bytes.

        vector<uchar> content;
        t->extractPayload(&content);

        map<string,pair<int,DVEntry>> vendor_values;

        // The first 8 bytes are error flags and a date time.
        // E.g. 0F005B5996000000 therefore we skip the first 8 bytes.
        //
        // 0F - Spcial function / packet
        // next 4B : Date - In default frame
        // next 3B : Faults - In default frame example: please see description of 0x01 register

        size_t i=8;
        while (i < content.size())
        {
            int c = content[i];
            int size = registerSize(c);
            if (c == 0xff) break; // An FF signals end of telegram padded to encryption boundary,
            // FFFFFFF623A where 4 last are perhaps crc or counter?
            i++;
            if (size == -1 || i+size > content.size())
            {
                vector<uchar> frame;
                t->extractFrame(&frame);
                string hex = bin2hex(frame);

                if (t->beingAnalyzed() == false)
                {
                    if (size == -1)
                    {
                        warning("(apator162) telegram contains a register (%02x) with unknown size.\n"
                                "Please open an issue at https://github.com/wmbusmeters/wmbusmeters/\n"
                                "and report this telegram: %s\n", c, hex.c_str());
                    }
                    else
                    {
                        warning("(apator162) telegram decoding fails since last register (%02x size %d) does not\n"
                                "align with telegram size %zu > %zu.\n"
                                "Please open an issue at https://github.com/wmbusmeters/wmbusmeters/\n"
                                "and report this telegram: %s\n", c, size, i+size, content.size(), hex.c_str());
                    }
                }
                break;
            }
            if (c == 0x10 && size == 4 && i+size < content.size())
            {
                // We found the register representing the total
                string total;
                strprintf(&total, "%02x%02x%02x%02x", content[i+0], content[i+1], content[i+2], content[i+3]);
                int offset = i-1+t->header_size;
                vendor_values["0413"] = {offset, DVEntry(offset, DifVifKey("0413"), MeasurementType::Instantaneous, 0x13, {}, {}, 0, 0, 0, total) };
                double total_water_consumption_m3 {};
                extractDVdouble(&vendor_values, "0413", &offset, &total_water_consumption_m3);
                total = "*** 10-"+total+" total consumption (%f m3)";
                t->addSpecialExplanation(offset, 4, KindOfData::CONTENT, Understanding::FULL, total.c_str(), total_water_consumption_m3);

                setNumericValue("total", Unit::M3, total_water_consumption_m3);
            }
            else
            {
                string msg = "*** ";
                msg += bin2hex(content, i-1, 1)+"-"+bin2hex(content, i, size);
                t->addSpecialExplanation(i-1+t->header_size, size, KindOfData::CONTENT, Understanding::NONE, msg.c_str());
            }
            i += size;
        }
    }

    int Driver::registerSize(int c)
    {
        switch (c)
        {
            // case 0x0f: return 3; // Payload often starts with 0x0f,
            // which  also means dif = manufacturer data follows.
            // After 0x0F there is always:
            // next 4B : Date - In default frame
            // next 3B : Faults - In default frame

        case 0x00: return 4; // Date
        case 0x01: return 3; // Faults - In default frame f.ex. 0F 09 4D A1 97 18 02 00 -> 18 02 00 -> 00 02 18 -> 0x0218

        case 0xA1:
        case 0x10: return 4; // Total volume - In default frame

        case 0x11: return 2; // Flow

        case 0x40: return 6; // Detectors
        case 0x41: return 2; // Voltage
        case 0x42: return 4; // Energy
        case 0x43: return 2; // Life days - In default frame f.ex. 43 6E 0A -> 2670 days from first run

        case 0x44: return 3;

        case 0x71: return 1+2*4; // ?
        case 0x72: return 1+3*4; // ?
        case 0x73: return 1+4*4; // Historical data
        case 0x75: return 1+6*4; // Historical data
        case 0x7B: return 1+12*4; // Historical data

        case 0x80:
        case 0x81:
        case 0x82:
        case 0x83:
        case 0x84:
        case 0x86:
        case 0x87: return 10; // Events

        case 0x85:
        case 0x88:
        case 0x8F: return 11; // Events

        case 0x8A: return 9; // Events

        case 0x8B:
        case 0x8C: return 6; // Events

        case 0x8E: return 7; // Events

        case 0xA0: return 4;

        case 0xA2: return 1;

        case 0xA3: return 7;

        case 0xA4: return 4;

        case 0xA5:
        case 0xA9:
        case 0xAF: return 1;

        case 0xA6: return 3;

        case 0xA7:
        case 0xA8:
        case 0xAA:
        case 0xAB:
        case 0xAC:
        case 0xAD: return 2;

        case 0xB0: return 5;
        case 0xB1: return 8;
        case 0xB2: return 16;
        case 0xB3: return 8;
        case 0xB4: return 2;
        case 0xB5: return 16;

        // Unknown
        case 0xB6: return 3;
        case 0xB7: return 3;
        case 0xB8: return 3;
        case 0xB9: return 3;
        case 0xBA: return 3;
        case 0xBB: return 3;
        case 0xBC: return 3;
        case 0xBD: return 3;
        case 0xBE: return 3;
        case 0xBF: return 3;

        case 0xC0: return 3;
        case 0xC1: return 3;
        case 0xC2: return 3;
        case 0xC3: return 3;
        case 0xC4: return 3;
        case 0xC5: return 3;
        case 0xC6: return 3;
        case 0xC7: return 3;

        case 0xD0: return 3;
        case 0xD3: return 3;

        case 0xF0: return 4;
        }
        return -1;
    }

    void Driver::processExtras(string miExtras)
    {
        map<string,string> extras;
        bool ok = parseExtras(miExtras, &extras);
        if (!ok)
        {
            error("(apator162) invalid extra parameters (%s)\n", miExtras.c_str());
        }
    }

}

// Test: Wasser apator162 20202020 NOKEY
// telegram=|6E4401062020202005077A9A006085|2F2F0F0A734393CC0000435B0183001A54E06F630291342510|030F00007B013E0B00003E0B00003E0B00003E0B00003E0B00003E0B00003E0B0000650000003D0000003D0000003D00000000000000A0910CB003FFFFFFFFFFFFFFFFFFFFA62B|
// {"media":"water","meter":"apator162","name":"Wasser","id":"20202020","total_m3":3.843,"timestamp":"1111-11-11T11:11:11Z"}
// |Wasser;20202020;3.843;1111-11-11 11:11.11

// Test: MyTapWatera apator162 21202020 NOKEY
// telegram=|4E4401062020202105077A13004085|2F2F0F6D4C389300020043840210|351F040075012C0B040048D603003E630300CD2C03001EF402000ACE0200A098A39603FFFFFFFFFFFFFFFFFFFFFFFFFF1977|
// {"media":"water","meter":"apator162","name":"MyTapWatera","id":"21202020","total_m3":270.133,"timestamp":"1111-11-11T11:11:11Z"}
// |MyTapWatera;21202020;270.133;1111-11-11 11:11.11

// Test: MyTapWaterb apator162 22202020 NOKEY
// telegram=|4E4401062020202205077A4B004085|2F2F0FE566B99390000087C0B24B732679FF75350010|FCFB00004155594265086A0043B4017301DFF600006AE70000BFD5000051BC0000A0F56C2602FFFF1B1B|
// {"media":"water","meter":"apator162","name":"MyTapWaterb","id":"22202020","total_m3":64.508,"timestamp":"1111-11-11T11:11:11Z"}
// |MyTapWaterb;22202020;64.508;1111-11-11 11:11.11

// Test: MyTapWaterc apator162 23202020 NOKEY
// telegram=|4E4401062020202305077A9D004085|2F2F0F81902C9300000010|B82F010041555942BD2882004319027301BC2601005C180100CB0A0100DFF60000A0F56C2602FFFFFFFFFFFFFFFFFFFFFFFFFF5B7C|
// {"media":"water","meter":"apator162","name":"MyTapWaterc","id":"23202020","total_m3":77.752,"timestamp":"1111-11-11T11:11:11Z"}
// |MyTapWaterc;23202020;77.752;1111-11-11 11:11.11

// Test: MyTapWaterd apator162 24202020 NOKEY
// telegram=|4E4401062020202405077A6C0040852F2F|0F73B3E19410000084E15381E553810101000010|FA41010041555942BF4E8A00433B027301AD380100BC2601005C180100CB0A0100A0F56C2602FFFFD0D7|
// {"media":"water","meter":"apator162","name":"MyTapWaterd","id":"24202020","total_m3":82.426,"timestamp":"1111-11-11T11:11:11Z"}
// |MyTapWaterd;24202020;82.426;1111-11-11 11:11.11

// Test: MyTapWatere apator162 25202020 NOKEY
// telegram=|4E4401062020202505077AEF0040852F2F|0F|071122|94|100200|43|6103|84|8B745953486C09100000|10|81920200|75|01F1800200E5640200534A02003431020080150200D9000200|A0|DC939703|FFFFA434|
// {"media":"water","meter":"apator162","name":"MyTapWatere","id":"25202020","total_m3":168.577,"timestamp":"1111-11-11T11:11:11Z"}
// |MyTapWatere;25202020;168.577;1111-11-11 11:11.11

// Test: MyTapWatere apator162 26202020 NOKEY
// telegram=|6E4401062020202605077AAC0060852F2F|0F|0C4442|94|1A0000|43|B502|83|000A549B4159029C290F|10|AB440000|7B|012C440000BE3E00008838000072340000493000009B2C00001D2C0000822B00007428000010250000B7200000261C0000|A0|A4D9A103|FFFFFFFFFFFFFFFFFFFF7823|
// {"media":"water","meter":"apator162","name":"MyTapWatere","id":"26202020","total_m3":17.579,"timestamp":"1111-11-11T11:11:11Z"}
// |MyTapWatere;26202020;17.579;1111-11-11 11:11.11

// telegram=|6E4401062020202605077AAD0060852F2F|0F|0E4442|94|1A0000|43|B502|84|4265594C655901010000|10|AB440000|7B|012C440000BE3E00008838000072340000493000009B2C00001D2C0000822B00007428000010250000B7200000261C0000|A0|A4D9A103|FFFFFFFFFFFFFFFFFFFF6C1B|
// {"media":"water","meter":"apator162","name":"MyTapWatere","id":"26202020","total_m3":17.579,"timestamp":"1111-11-11T11:11:11Z"}
// |MyTapWatere;26202020;17.579;1111-11-11 11:11.11

// telegram=|6E4401062020202605077AAE0060852F2F|0F|0F4442|94|1A0000|43|B502|81|D87F57D87F5701010000|10|AB440000|7B|012C440000BE3E00008838000072340000493000009B2C00001D2C0000822B00007428000010250000B7200000261C0000|A0|A4D9A103|FFFFFFFFFFFFFFFFFFFF5F22|
// {"media":"water","meter":"apator162","name":"MyTapWatere","id":"26202020","total_m3":17.579,"timestamp":"1111-11-11T11:11:11Z"}
// |MyTapWatere;26202020;17.579;1111-11-11 11:11.11

// Test: MyTapWaterf apator162 03410514 NOKEY
// telegram=|3E4401061405410305077A190030852F2F|0F|86B4B8|95|290200|40|C6C1|B4|F0F3F3|41|5559|42|FA701000|F0|01010000|10|BC780000|FFFFFFFFFFFFFFFFFFFFFF2483|
// {"media":"water","meter":"apator162","name":"MyTapWaterf","id":"03410514","total_m3":30.908,"timestamp":"1111-11-11T11:11:11Z"}
// |MyTapWaterf;03410514;30.908;1111-11-11 11:11.11

// Test: MyTapWaterg apator162 27202020 NOKEY
// telegram=|6E4401062020202705077A3D0060852F2F|0F|151794|94|0A0200|43|0403|81|D87F57D87F5701010000|10|783E0000|7B01223C00009137000098320000392D000010290000F02600004C2400003422000004220000CB21000017200000C51C0000|A0|9AD9A103|FFFFFFFFFFFFFFFFFFFF367E|
// {"media":"water","meter":"apator162","name":"MyTapWaterg","id":"27202020","total_m3":15.992,"timestamp":"1111-11-11T11:11:11Z"}
// |MyTapWaterg;27202020;15.992;1111-11-11 11:11.11


// Test: NewAndOld apator162 00148686 NOKEY
// Comment: New apator162 telegram which can be decoded.
// telegram=4E4401068686140005077A350040852F2F_0F005B599600000010AA55000041545A42850BD800437D037301C5500000564B00009E4600006A410000A01778EC03FFFFFFFFFFFFFFFFFFFFFFFFFFE393
// {"media":"water","meter":"apator162","name":"NewAndOld","id":"00148686","total_m3":21.93,"timestamp":"1111-11-11T11:11:11Z"}
// |NewAndOld;00148686;21.93;1111-11-11 11:11.11

// Comment: Old style apator162 telegram which cannot be decoded.
// telegram=5A441486868614000507B6_0AFFFFF5450106F41BAD717A35004085C90AC6D97E3294827563E70F4CF00655FC796A76B87AD1D4A69D16F5EDD1084318F46559E43D2C60D2B1CE581D0CAC1BBC73A376B9D71F0D71C6C904B04DC30E
// This telegram should not trigger any shell or other output!

// telegram=4E4401068686140005077A350040852F2F_0F005B599600000010AA66000041545A42850BD800437D037301C5500000564B00009E4600006A410000A01778EC03FFFFFFFFFFFFFFFFFFFFFFFFFFE393
// {"media":"water","meter":"apator162","name":"NewAndOld","id":"00148686","total_m3":26.282,"timestamp":"1111-11-11T11:11:11Z"}
// |NewAndOld;00148686;26.282;1111-11-11 11:11.11


// Test: has80 apator162 04040404 NOKEY
// telegram=|704401060404040405077A0E0060852F2F_0F766DFB96010000430600808F67DB8F67DB01010000102F1F01007B01000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000A05F5C1804FFFFFFFFFFFFFFFFFFFF26BCD649|
// {"media":"water","meter":"apator162","name":"has80","id":"04040404","total_m3":73.519,"timestamp":"1111-11-11T11:11:11Z"}
// |has80;04040404;73.519;1111-11-11 11:11.11
