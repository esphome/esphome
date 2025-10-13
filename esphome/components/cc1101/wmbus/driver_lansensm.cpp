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
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("lansensm");
        di.setDefaultFields("name,id,status,minutes_since_last_manual_test_counter,timestamp");
        di.setMeterType(MeterType::SmokeDetector);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_LAS, 0x1a, 0x03);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addStringFieldWithExtractorAndLookup(
            "status",
            "Meter status.",
            DEFAULT_PRINT_PROPERTIES   |
            PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS,
            FieldMatcher::build()
            .set(VIFRange::ErrorFlags)
            .add(VIFCombinable::StandardConformantDataContent),
            Translate::Lookup(
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::MapType::BitToString,
                        AlwaysTrigger, MaskBits(0xffff),
                        "OK",
                        {
                            { 0x0002, "LOW_BATTERY" },
                            { 0x0004, "SMOKE" },
                            { 0x0008, "MANUAL_TEST" },
                            { 0x0010, "MALFUNCTION" },
                            { 0x0020, "NO_CONNECTION_TO_SMOKE_DETECTOR" },
                            { 0x0100, "SMOKE_SENSOR_END_OF_LIFE" }
                        }
                    },
                },
            }));

        addNumericFieldWithExtractor(
            "async_msg_id",
            "Unique asynchronous message number.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AccessNumber)
            );

        addNumericFieldWithExtractor(
            "minutes_since_last_manual_test",
            "Minutes since last manual test.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Dimensionless)
            );

    }
}

// Test: SMOKEA lansensm 00010204 NOKEY
// telegram=|2E44333004020100031A7AC40020052F2F_02FD971D000004FD084C02000004FD3A467500002F2F2F2F2F2F2F2F2F2F|
// {"media":"smoke detector","meter":"lansensm","name":"SMOKEA","id":"00010204","status":"OK","async_msg_id_counter":588,"minutes_since_last_manual_test_counter":30022,"timestamp":"1111-11-11T11:11:11Z"}
// |SMOKEA;00010204;OK;30022;1111-11-11 11:11.11

// telegram=|2E44333004020100031A7ADE0020052F2F_02FD971D040004FD086502000004FD3A010000002F2F2F2F2F2F2F2F2F2F|
// {"media":"smoke detector","meter":"lansensm","name":"SMOKEA","id":"00010204","status":"SMOKE","async_msg_id_counter":613,"minutes_since_last_manual_test_counter":1,"timestamp":"1111-11-11T11:11:11Z"}
// |SMOKEA;00010204;SMOKE;1;1111-11-11 11:11.11
