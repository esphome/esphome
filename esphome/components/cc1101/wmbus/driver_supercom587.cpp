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
        di.setName("supercom587");
        di.setDefaultFields("name,id,total_m3,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_SON, 0x06,  0x3c);
        di.addDetection(MANUFACTURER_SON, 0x07,  0x3c);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addOptionalLibraryFields("software_version");
        addOptionalLibraryFields("total_m3");

        addStringFieldWithExtractorAndLookup(
            "status",
            "Status of meter.",
            DEFAULT_PRINT_PROPERTIES | PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ErrorFlags),
            Translate::Lookup()
            .add(Translate::Rule("ERROR_FLAGS", Translate::MapType::BitToString)
                 .set(MaskBits(0x000f))
                 .set(DefaultMessage("OK"))
                ));
    }
}

// Test: MyWarmWater supercom587 12345678 NOKEY
// telegram=|A244EE4D785634123C067A8F000000_0C1348550000426CE1F14C130000000082046C21298C0413330000008D04931E3A3CFE3300000033000000330000003300000033000000330000003300000033000000330000003300000033000000330000004300000034180000046D0D0B5C2B03FD6C5E150082206C5C290BFD0F0200018C4079678885238310FD3100000082106C01018110FD610002FD66020002FD170000|
// {"media":"warm water","meter":"supercom587","name":"MyWarmWater","id":"12345678","total_m3":5.548,"software_version":"010002","status":"OK","timestamp":"1111-11-11T11:11:11Z"}
// |MyWarmWater;12345678;5.548;1111-11-11 11:11.11

// Test: MyColdWater supercom587 11111111 NOKEY
// telegram=|A244EE4D111111113C077AAC000000_0C1389490000426CE1F14C130000000082046C21298C0413010000008D04931E3A3CFE0100000001000000010000000100000001000000010000000100000001000000010000000100000001000000010000001600000031130000046D0A0C5C2B03FD6C60150082206C5C290BFD0F0200018C4079629885238310FD3100000082106C01018110FD610002FD66020002FD170000|
// {"media":"water","meter":"supercom587","name":"MyColdWater","id":"11111111","total_m3":4.989,"software_version":"010002","status":"OK","timestamp":"1111-11-11T11:11:11Z"}
// |MyColdWater;11111111;4.989;1111-11-11 11:11.11
