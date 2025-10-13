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
    };

    static bool ok = registerDriver([](DriverInfo&di)

    {
        di.setName("mkradio4a");
        di.setDefaultFields("name,id,target_m3,target_date,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::T1);
        di.addLinkMode(LinkMode::C1);
        di.addDetection(MANUFACTURER_HYD, 0x06,  0xfe);
        di.addDetection(MANUFACTURER_TCH, 0x37,  0x95);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

        addNumericFieldWithExtractor(
            "target",
            "The total water consumption recorded at the end of previous year.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "target",
            "Date when previous year ended.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::PointInTime,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1)),
            Unit::DateLT
            );
    }
}
// Test: FOO mkradio4a 66953825 NOKEY
// Comment: Warm water
// telegram=|4B44685036494600953772253895662423FE064E0030052F2F_4315A10000426CBF2C0F542CF2DD8BEC869511B2DB8301C3ABA390FB4FDB6F1144DA1F3897DD55F2AD0D194F68510FF8FADFB9|
// {"media":"warm water","meter":"mkradio4a","name":"FOO","id":"66953825","target_m3":16.1,"target_date":"2021-12-31","timestamp":"1111-11-11T11:11:11Z"}
// |FOO;66953825;16.1;2021-12-31;1111-11-11 11:11.11

// Test: BAR mkradio4a 01770002 NOKEY
// Comment: Cold water
// telegram=|4B4468508644710095377202007701A85CFE078A0030052F2F_4315F00200426CBF2C0FEE456BF6F802216503E25EB73E9377D54F672681B76C469696E4C7BCCC9072CC79F712360FC3F57D85|
// {"media":"water","meter":"mkradio4a","name":"BAR","id":"01770002","target_m3":75.2,"target_date":"2021-12-31","timestamp":"1111-11-11T11:11:11Z"}
// |BAR;01770002;75.2;2021-12-31;1111-11-11 11:11.11
