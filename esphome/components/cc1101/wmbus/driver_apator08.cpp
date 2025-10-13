/*
 Copyright (C) 2020-2022 Fredrik Öhrström (gpl-3.0-or-later)

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

    private:

        void processContent(Telegram *t);
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("apator08");
        di.setDefaultFields("name,id,total_m3,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_APT, 0x03,  0x03);
        di.addDetection(MANUFACTURER_APT, 0x0F, 0x0F);
        di.addDetection(0x8614 /*APT?*/, 0x03,  0x03);
        di.usesProcessContent();
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addNumericField(
            "total",
            Quantity::Volume,
            DEFAULT_PRINT_PROPERTIES,
            "The total water consumption recorded by this meter.");
    }

    void Driver::processContent(Telegram *t)
    {
        // The telegram says gas (0x03) but it is a water meter.... so fix this.
        t->dll_type = 0x07;

        vector<uchar> content;
        t->extractPayload(&content);

        if (content.size() < 4) return;

        map<string,pair<int,DVEntry>> vendor_values;

        string total;
        strprintf(&total, "%02x%02x%02x%02x", content[0], content[1], content[2], content[3]);

        vendor_values["0413"] = { 25, DVEntry(25, DifVifKey("0413"), MeasurementType::Instantaneous, 0x13, {}, {}, 0, 0, 0, total) };
        int offset;
        string key;
        if(findKey(MeasurementType::Instantaneous, VIFRange::Volume, 0, 0, &key, &vendor_values))
        {
            double total_water_consumption_m3 {};
            extractDVdouble(&vendor_values, key, &offset, &total_water_consumption_m3);
            // Now divide with 3! Is this the same for all apator08 meters? Time will tell.
            total_water_consumption_m3 /= 3.0;

            total = "*** 10|"+total+" total consumption (%f m3)";
            t->addSpecialExplanation(offset, 4, KindOfData::CONTENT, Understanding::FULL, total.c_str(), total_water_consumption_m3);

            setNumericValue("total", Unit::M3, total_water_consumption_m3);
        }
    }
}

// Test: Vatten apator08 004444dd NOKEY
// telegram=|73441486DD4444000303A0B9E527004C4034B31CED0106FF01D093270065F022009661230054D02300EC49240018B424005F012500936D2500FFD525000E3D26001EAC26000B2027000300000000371D0B2000000000000024000000000000280000000000002C0033150C010D2F000000000000|
// {"media":"water","meter":"apator08","name":"Vatten","id":"004444dd","total_m3":871.571,"timestamp":"1111-11-11T11:11:11Z"}
// |Vatten;004444dd;871.571;1111-11-11 11:11.11

// Test: test_apator082 apator08 00149c06 NOKEY
// telegram=|_1C441486069C14000F0FA042F214000040030000000005FF0472BF1400|
// {"media":"water","meter":"apator08","name":"test_apator082","id":"00149c06","total_m3":457.579333,"timestamp":"1111-11-11T11:11:11Z"}
// |test_apator082;00149c06;457.579333;1111-11-11 11:11.11
