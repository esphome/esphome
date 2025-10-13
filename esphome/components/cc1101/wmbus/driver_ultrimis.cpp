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
    struct Driver : public virtual MeterCommonImplementation {
        Driver(MeterInfo &mi, DriverInfo &di);
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("ultrimis");
        di.setDefaultFields("name,id,total_m3,target_m3,current_status,total_backward_flow_m3,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_APA,  0x16,  0x01);
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
             "The total water consumption recorded at the beginning of this month.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(1))
            );

        addStringFieldWithExtractorAndLookup(
            "current_status",
            "Status and error flags.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(DifVifKey("03FD17")),
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::MapType::BitToString,
                        AlwaysTrigger, MaskBits(0xffffff),
                        "OK",
                        {
                            /* What are the bits?
                               According to the manual this meter offers these alarms:
                               Back flow
                               Meter leak
                               Water main leak
                               Zero flow
                               Tampering detected
                               No water
                               Low battery
                            */
                        }
                    },
                },
            });

        addNumericFieldWithExtractor(
            "total_backward_flow",
            "The total backward water volume recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("04933C"))
            );
    }
}

// Test: Water ultrimis 95969798 NOKEY
// Comment:
// telegram=|2E4401069897969501167A4B0320052F2F_0413320C000003FD1700000044132109000004933C000000002F2F2F2F2F|
// {"media":"cold water","meter":"ultrimis","name":"Water","id":"95969798","total_m3":3.122,"target_m3":2.337,"current_status":"OK","total_backward_flow_m3":0,"timestamp":"1111-11-11T11:11:11Z"}
// |Water;95969798;3.122;2.337;OK;0;1111-11-11 11:11.11
