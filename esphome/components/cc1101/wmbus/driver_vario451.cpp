/*
 Copyright (C) 2019-2022 Fredrik Öhrström (gpl-3.0-or-later)

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
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("vario451");
        di.setDefaultFields("name,id,total_kwh,current_kwh,previous_kwh,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::C1);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_TCH, 0x04,  0x27);
        di.addDetection(MANUFACTURER_TCH, 0xc3,  0x27);
        di.usesProcessContent();
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addNumericField("total",
                        Quantity::Energy,
                         DEFAULT_PRINT_PROPERTIES,
                        "The total energy consumption recorded by this meter.");

        addNumericField("current",
                        Quantity::Energy,
                         DEFAULT_PRINT_PROPERTIES,
                        "Energy consumption so far in this billing period.");

        addNumericField("previous",
                        Quantity::Energy,
                         DEFAULT_PRINT_PROPERTIES,
                        "Energy consumption in previous billing period.");
    }

    void Driver::processContent(Telegram *t)
    {
        // Unfortunately, the Techem Vario 4 Typ 4.5.1 is mostly a proprieatary protocol
        // simple wrapped inside a wmbus telegram since the ci-field is 0xa2.
        // Which means that the entire payload is manufacturer specific.

        map<string,pair<int,DVEntry>> vendor_values;
        vector<uchar> content;

        t->extractPayload(&content);
        if (content.size() < 9) return;

        uchar prev_lo = content[3];
        uchar prev_hi = content[4];
        double prev_gj = (256.0*prev_hi+prev_lo)/1000;

        string prevs;
        strprintf(&prevs, "%02x%02x", prev_lo, prev_hi);
        int offset = t->parsed.size()+3;
        vendor_values["0215"] = { offset, DVEntry(offset, DifVifKey("0215"), MeasurementType::Instantaneous, 0x15, {}, {}, 0, 0, 0, prevs) };
        t->explanations.push_back(Explanation(offset, 2, prevs, KindOfData::CONTENT, Understanding::FULL));
        t->addMoreExplanation(offset, " energy used in previous billing period (%f GJ)", prev_gj);

        uchar curr_lo = content[7];
        uchar curr_hi = content[8];
        double curr_gj = (256.0*curr_hi+curr_lo)/1000;

        string currs;
        strprintf(&currs, "%02x%02x", curr_lo, curr_hi);
        offset = t->parsed.size()+7;
        vendor_values["0215"] = { offset, DVEntry(offset, DifVifKey("0215"), MeasurementType::Instantaneous, 0x15, {}, {}, 0, 0, 0, currs) };
        t->explanations.push_back(Explanation(offset, 2, currs, KindOfData::CONTENT, Understanding::FULL));
        t->addMoreExplanation(offset, " energy used in current billing period (%f GJ)", curr_gj);

        setNumericValue("total", Unit::GJ, curr_gj+prev_gj);
        setNumericValue("current", Unit::GJ, curr_gj);
        setNumericValue("previous", Unit::GJ, prev_gj);
    }

}

// Test: HeatMeter vario451 58234965 NOKEY
// telegram=|374468506549235827C3A2_129F25383300A8622600008200800A2AF862115175552877A36F26C9AB1CB24400000004000000000004908002|
// {"media":"heat","meter":"vario451","name":"HeatMeter","id":"58234965","total_kwh":6371.666667,"current_kwh":2729.444444,"previous_kwh":3642.222222,"timestamp":"1111-11-11T11:11:11Z"}
// |HeatMeter;58234965;6371.666667;2729.444444;3642.222222;1111-11-11 11:11.11
