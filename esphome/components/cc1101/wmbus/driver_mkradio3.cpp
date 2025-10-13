/*
 Copyright (C) 2019-2023 Fredrik Öhrström (gpl-3.0-or-later)

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
        di.setName("mkradio3");
        di.setDefaultFields("name,id,total_m3,target_m3,current_date,prev_date,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_TCH, 0x62,  0x74);
        di.addDetection(MANUFACTURER_TCH, 0x72,  0x74);
        di.usesProcessContent();
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addNumericField("total",
                        Quantity::Volume,
                        DEFAULT_PRINT_PROPERTIES,
                        "The total water consumption recorded by this meter.");

        addNumericField("target",
                        Quantity::Volume,
                        DEFAULT_PRINT_PROPERTIES,
                        "The total water consumption recorded at the beginning of this month.");

        addStringField("current_date",
                       "Date of current billing period.",
                       DEFAULT_PRINT_PROPERTIES);

        addStringField("prev_date",
                       "Date of previous billing period.",
                       DEFAULT_PRINT_PROPERTIES);
    }

    void Driver::processContent(Telegram *t)
    {
        // Unfortunately, the MK Radio 3 is mostly a proprieatary protocol
        // simple wrapped inside a wmbus telegram since the ci-field is 0xa2.
        // Which means that the entire payload is manufacturer specific.

        map<string,pair<int,DVEntry>> vendor_values;
        vector<uchar> content;

        t->extractPayload(&content);

        // Previous date
        uint16_t prev_date = (content[2] << 8) | content[1];
        uint prev_date_day = (prev_date >> 0) & 0x1F;
        uint prev_date_month = (prev_date >> 5) & 0x0F;
        uint prev_date_year = (prev_date >> 9) & 0x3F;
        setStringValue("prev_date",
                       tostrprintf("%d-%02d-%02dT02:00:00Z", prev_date_year+2000, prev_date_month, prev_date_day));

        // Previous consumption
        uchar prev_lo = content[3];
        uchar prev_hi = content[4];
        double prev = (256.0*prev_hi+prev_lo)/10.0;

        // Current date
        uint16_t current_date = (content[6] << 8) | content[5];
        uint current_date_day = (current_date >> 4) & 0x1F;
        uint current_date_month = (current_date >> 9) & 0x0F;
        setStringValue("current_date",
                       tostrprintf("%s-%02d-%02dT02:00:00Z",
                                   currentYear().c_str(), current_date_month, current_date_day));

        // Current consumption
        uchar curr_lo = content[7];
        uchar curr_hi = content[8];
        double curr = (256.0*curr_hi+curr_lo)/10.0;

        double total_water_consumption_m3 = prev+curr;
        setNumericValue("total", Unit::M3, total_water_consumption_m3);
        double target_water_consumption_m3 = prev;
        setNumericValue("target", Unit::M3, target_water_consumption_m3);
    }
}

// Test: Duschen mkradio3 34333231 NOKEY
// Comment: There is a problem in the decoding here, the data stored inside the telegram does not seem to properly encode/decode the year....
// We should not report a current_date with a full year, if the year is actually not part of the telegram.
// telegram=|2F446850313233347462A2_069F255900B029310000000306060906030609070606050509050505050407040605070500|
// {"media":"warm water","meter":"mkradio3","name":"Duschen","id":"34333231","total_m3":13.8,"target_m3":8.9,"current_date":"2024-04-27T02:00:00Z","prev_date":"2018-12-31T02:00:00Z","timestamp":"1111-11-11T11:11:11Z"}
// |Duschen;34333231;13.8;8.9;2024-04-27T02:00:00Z;2018-12-31T02:00:00Z;1111-11-11 11:11.11
