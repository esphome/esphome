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

        void processContent(Telegram *t);
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("compact5");
        di.setDefaultFields("name,id,total_kwh,current_kwh,previous_kwh,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::T1);
        di.addLinkMode(LinkMode::C1);
        di.addDetection(MANUFACTURER_TCH,  0x04,  0x45);
        di.addDetection(MANUFACTURER_TCH,  0xc3,  0x45);
        di.addDetection(MANUFACTURER_TCH,  0x43,  0x22);
        di.addDetection(MANUFACTURER_TCH,  0x43,  0x45);
        di.addDetection(MANUFACTURER_TCH,  0x43,  0x39);
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
        // Unfortunately, the Techem Compact V is mostly a proprieatary protocol
        // simple wrapped inside a wmbus telegram since the ci-field is 0xa2.
        // Which means that the entire payload is manufacturer specific.

        map<string,pair<int,DVEntry>> vendor_values;
        vector<uchar> content;

        t->extractPayload(&content);

        if (content.size() < 9) return;

        uchar prev_lo = content[3];
        uchar prev_hi = content[4];
        double prev = (256.0*prev_hi+prev_lo);

        string prevs;
        strprintf(&prevs, "%02x%02x", prev_lo, prev_hi);
        int offset = t->parsed.size()+3;
        vendor_values["0215"] = { offset, DVEntry(offset, DifVifKey("0215"), MeasurementType::Instantaneous, 0x15, {}, {}, 0, 0, 0, prevs) };
        Explanation pe(offset, 2, prevs, KindOfData::CONTENT, Understanding::FULL);
        t->explanations.push_back(pe);
        t->addMoreExplanation(offset, " energy used in previous billing period (%f KWH)", prev);

        uchar curr_lo = content[7];
        uchar curr_hi = content[8];
        double curr = (256.0*curr_hi+curr_lo);

        string currs;
        strprintf(&currs, "%02x%02x", curr_lo, curr_hi);
        offset = t->parsed.size()+7;
        vendor_values["0215"] = { offset, DVEntry(offset, DifVifKey("0215"), MeasurementType::Instantaneous, 0x15, {}, {}, 0, 0, 0, currs) };
        Explanation ce(offset, 2, currs, KindOfData::CONTENT, Understanding::FULL);
        t->explanations.push_back(ce);
        t->addMoreExplanation(offset, " energy used in current billing period (%f KWH)", curr);

        double total_energy_kwh = prev+curr;
        setNumericValue("total", Unit::KWH, total_energy_kwh);

        double curr_energy_kwh = curr;
        setNumericValue("current", Unit::KWH, curr_energy_kwh);

        double prev_energy_kwh = prev;
        setNumericValue("previous", Unit::KWH, prev_energy_kwh);
    }
}

// Test: Heating compact5 62626262 NOKEY
// telegram=|36446850626262624543A1_009F2777010060780000000A000000000000000000000000000000000000000000000000A0400000B4010000|
// {"media":"heat","meter":"compact5","name":"Heating","id":"62626262","total_kwh":495,"current_kwh":120,"previous_kwh":375,"timestamp":"1111-11-11T11:11:11Z"}
// |Heating;62626262;495;120;375;1111-11-11 11:11.11

// Test: Heating2 compact5 66336633 NOKEY
// telegram=|37446850336633663943a2_10672c866100181c01000480794435d50000000000000000000000000000000000000000000000000000000000|
// {"media":"heat","meter":"compact5","name":"Heating2","id":"66336633","total_kwh":25250,"current_kwh":284,"previous_kwh":24966,"timestamp":"1111-11-11T11:11:11Z"}
// |Heating2;66336633;25250;284;24966;1111-11-11 11:11.11
