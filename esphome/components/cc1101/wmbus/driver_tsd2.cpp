/*
 Copyright (C) 2020-2023 Fredrik Öhrström (gpl-3.0-or-later)

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
        di.setName("tsd2");
        di.setDefaultFields("name,id,status,prev_date,timestamp");
        di.setMeterType(MeterType::SmokeDetector);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_TCH, 0xf0,  0x76);
        di.usesProcessContent();
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addStringField("status",
                       "The current status: OK, SMOKE or ERROR.",
                       DEFAULT_PRINT_PROPERTIES);

        addStringField("prev_date",
                       "Date of previous billing period.",
                       DEFAULT_PRINT_PROPERTIES);
    }

#define INFO_CODE_SMOKE 0x01

    void Driver::processContent(Telegram *t)
    {
        vector<uchar> data;
        t->extractPayload(&data);

        if(data.size() < 3)
        {
            setStringValue("status", "ERROR");
            return;
        }

        uint8_t status = data[0];
        if (status & INFO_CODE_SMOKE)
        {
            setStringValue("status", "SMOKE");
        }
        else
        {
            setStringValue("status", "OK");
        }

        uint16_t prev_date = (data[2] << 8) | data[1];
        uint prev_date_day = (prev_date >> 0) & 0x1F;
        uint prev_date_month = (prev_date >> 5) & 0x0F;
        uint prev_date_year = (prev_date >> 9) & 0x3F;

        setStringValue("prev_date",
                       tostrprintf("%d-%02d-%02dT02:00:00Z", prev_date_year+2000, prev_date_month, prev_date_day));
    }
}

// Test: Smokey tsd2 91633569 NOKEY

// telegram=|294468506935639176F0A0_019F|
// {"media":"smoke detector","meter":"tsd2","name":"Smokey","id":"91633569","status":"ERROR","timestamp":"1111-11-11T11:11:11Z"}
// |Smokey;91633569;ERROR;null;1111-11-11 11:11.11

// telegram=|294468506935639176F0A0_009F2782290060822900000401D6311AF93E1BF93E008DC3009ED4000FE500|
// {"media":"smoke detector","meter":"tsd2","name":"Smokey","id":"91633569","status":"OK","prev_date":"2019-12-31T02:00:00Z","timestamp":"1111-11-11T11:11:11Z"}
// |Smokey;91633569;OK;2019-12-31T02:00:00Z;1111-11-11 11:11.11

// telegram=|294468506935639176F0A0_019F2782290060822900000401D6311AF93E1BF93E008DC3009ED4000FE500|
// {"media":"smoke detector","meter":"tsd2","name":"Smokey","id":"91633569","status":"SMOKE","prev_date":"2019-12-31T02:00:00Z","timestamp":"1111-11-11T11:11:11Z"}
// |Smokey;91633569;SMOKE;2019-12-31T02:00:00Z;1111-11-11 11:11.11
