/*
 Copyright (C) 2023 Fredrik Öhrström (gpl-3.0-or-later)

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
        di.setName("iwmtx5");
        di.setDefaultFields("name,id,status,total_m3,timestamp");

        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_BMT, 0x07, 0x18);
        di.addDetection(MANUFACTURER_BMT, 0x06, 0x18);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addOptionalLibraryFields("meter_datetime");
        addOptionalLibraryFields("total_m3");

        addStringField(
            "status",
            "Status and error flags.",
            PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS);
    }
}

// Test: WaterWater iwmtx5 22917370 00000000000000000000000000000000
// telegram=|5144b4097073912218078c00247a0308400571e9615249ede52eaae09f61908f027c3877f3330ae9079528b23173ce124bcc255393e60b173c0a9f274c42dd92e4b23c14e8a41f042903358df01dd9268ad4|
// {"id": "22917370","media": "water","meter": "iwmtx5","meter_datetime": "2023-05-11 10:38:24","name": "WaterWater","status": "PERMANENT_ERROR","timestamp": "1111-11-11T11:11:11Z","total_m3": 0.025}
// |WaterWater;22917370;PERMANENT_ERROR;0.025;1111-11-11 11:11.11

// Test: WarmWater2 iwmtx5 23329344 NOKEY
// telegram=|4244B4094493322318068C005B7A1C0000000C13072000000F05170000000000000000000000000000000000000000009D0000C20000C20000C8000000000000000000|
// {"id": "23329344","media": "warm water","meter": "iwmtx5","name": "WarmWater2","status": "OK","timestamp": "1111-11-11T11:11:11Z","total_m3": 2.007}
// |WarmWater2;23329344;OK;2.007;1111-11-11 11:11.11
