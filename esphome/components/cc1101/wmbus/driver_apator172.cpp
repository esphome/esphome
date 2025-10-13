/*
 Copyright (C) 2022 Fredrik Öhrström (gpl-3.0-or-later)

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

#include <cmath>

namespace
{
    struct Driver : public virtual MeterCommonImplementation
    {
        Driver(MeterInfo &mi, DriverInfo &di);

        void processContent(Telegram *t);
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("apator172");
        di.setDefaultFields("name,id,total_m3,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addDetection(0x8614 /*APT?*/,  0x11,  0x04);
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
        vector<uchar> content;
        t->extractPayload(&content);

        // Overwrite the non-standard 0x11 with 0x07 which means water.
        t->dll_type = 0x07;

        map<string,pair<int,DVEntry>> vendor_values;

        size_t i=0;
        if (i+4 < content.size())
        {
            // We found the register representing the total
            string total;
            strprintf(&total, "%02x%02x%02x%02x", content[i+0], content[i+1], content[i+2], content[i+3]);
            int offset = i-1+t->header_size;
            vendor_values["0413"] = {offset, DVEntry(offset, DifVifKey("0413"), MeasurementType::Instantaneous, 0x13, {}, {}, 0, 0, 0, total) };
            double tmp = 0;
            extractDVdouble(&vendor_values, "0413", &offset, &tmp);
            // Single tick seems to be 1/3 of a m3. Divide by 3 and keep a single decimal.
            // This will report consumption as 100.0 100.3 100.7 101.0 etc.
            double total_water_consumption_m3 = 0.1*std::round(10000.0*tmp/3.0);
            total = "*** "+total+" total consumption (%f m3)";
            t->addSpecialExplanation(offset, 4, KindOfData::CONTENT, Understanding::FULL, total.c_str(), total_water_consumption_m3);

            setNumericValue("total", Unit::M3, total_water_consumption_m3);
        }
    }
}

// Test: Vattur apator172 0014a807 NOKEY
// telegram=|1C44148607A814000411A0_1D5400000840030000000005FF05D83D0000|
// {"media":"water","meter":"apator172","name":"Vattur","id":"0014a807","total_m3":7177.7,"timestamp":"1111-11-11T11:11:11Z"}
// |Vattur;0014a807;7177.7;1111-11-11 11:11.11

// telegram=|1C44148607A814000411A0_215400000840030000000005FF05D83D0000|
// {"media":"water","meter":"apator172","name":"Vattur","id":"0014a807","total_m3":7179,"timestamp":"1111-11-11T11:11:11Z"}
// |Vattur;0014a807;7179;1111-11-11 11:11.11
