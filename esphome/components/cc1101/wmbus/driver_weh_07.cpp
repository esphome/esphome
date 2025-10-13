/*
 Copyright (C) 2021-2022 Fredrik Öhrström (gpl-3.0-or-later)

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
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("weh_07");
        di.setDefaultFields("name,id,total_m3,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::C1);
        di.addDetection(MANUFACTURER_WEH, 0x07,  0xfe);
        di.addDetection(MANUFACTURER_WEH, 0x07,  0x03);

        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addNumericFieldWithExtractor(
            "total",
            "The total water consumption recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            );

        addNumericFieldWithExtractor(
            "target",
            "The most recent billing period date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::PointInTime,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1)),
            Unit::DateLT
            );

        addNumericFieldWithExtractor(
            "target",
            "The total water consumption at the most recent billing period date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(1))
            );
    }
}

// Test: Vatten weh_07 86868686 NOKEY
// Comment: Techem radio convert + Wehrle water meter combo.
// telegram=|494468509494949495377286868686A85CFE07A90030052F2F_0413100000000F52FCF6A52A90A8D83CA8F7FEAE86990502323D0C70EFF49833C7C1696F75BCABC1E52E6305308D0F31FB|
// {"media":"water","meter":"weh_07","name":"Vatten","id":"86868686","total_m3":0.016,"timestamp":"1111-11-11T11:11:11Z"}
// |Vatten;86868686;0.016;1111-11-11 11:11.11

// Test: Vattenn weh_07 27604781 NOKEY
// Comment: A normal water meter.
// telegram=|5244A85C8147602703077A5B0840252F2F_0413B39100004413000000004D931E2C73FE0000000000000000000000000000000000000000000000000000000000000000000000009885001A0C002F2F426CBE29|
// {"id": "27604781","media": "water","meter": "weh_07","name": "Vattenn","target_date": "2021-09-30","target_m3": 0,"timestamp": "1111-11-11T11:11:11Z","total_m3": 37.299 }
// |Vattenn;27604781;37.299;1111-11-11 11:11.11
