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
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("hcae2");
        di.setDefaultFields("name,id,current_consumption_hca,status,timestamp");
        di.setMeterType(MeterType::HeatCostAllocationMeter);
        di.addLinkMode(LinkMode::T1);
        di.addLinkMode(LinkMode::C1);
        di.addDetection(MANUFACTURER_EFE, 0x08, 0x31);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addStringFieldWithExtractorAndLookup(
            "status",
            "Meter status from error flags and tpl status field.",
            DEFAULT_PRINT_PROPERTIES  |
            PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ErrorFlags),
            Translate::Lookup(
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::MapType::BitToString,
                        AlwaysTrigger, MaskBits(0xffff),
                        "OK",
                        {
                            { 0x0001, "MEASUREMENT" },
                            { 0x0002, "SABOTAGE" },
                            { 0x0004, "BATTERY" },
                            { 0x0008, "CS" },
                            { 0x0010, "HF" },
                            { 0x0020, "RESET" }
                        }
                    },
                },
            }));

        addNumericFieldWithExtractor(
            "current_consumption",
            "The current heat cost allocation.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::HCA,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::HeatCostAllocation)
            );

        addNumericFieldWithExtractor(
            "consumption_at_set_date_{storage_counter}",
            "The heat cost allocation at set date #.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::HCA,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::HeatCostAllocation)
            .set(StorageNr(1),StorageNr(17))
            );

        addNumericFieldWithExtractor(
            "consumption_at_set_date",
            "Deprecated field.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::HCA,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::HeatCostAllocation)
            .set(StorageNr(1))
            );


    }
}

// Test: HeatMeter hcae2 88018801 NOKEY
// telegram=|7644C52501880188550872_01880188C5255508010000002F2F0B6E332211426E110182016E1102C2016E110382026E1104C2026E110582036E1106C2036E110782046E1108C2046E110982056E1110C2056E111182066E1112C2066E111382076E1114C2076E111582086E1116C2086E111702FD172100|
// {"media":"heat cost allocation","meter":"hcae2","name":"HeatMeter","id":"88018801","status":"MEASUREMENT RESET","current_consumption_hca":112233,"consumption_at_set_date_hca":273,"consumption_at_set_date_1_hca":273,"consumption_at_set_date_2_hca":529,"consumption_at_set_date_3_hca":785,"consumption_at_set_date_4_hca":1041,"consumption_at_set_date_5_hca":1297,"consumption_at_set_date_6_hca":1553,"consumption_at_set_date_7_hca":1809,"consumption_at_set_date_8_hca":2065,"consumption_at_set_date_9_hca":2321,"consumption_at_set_date_10_hca":4113,"consumption_at_set_date_11_hca":4369,"consumption_at_set_date_12_hca":4625,"consumption_at_set_date_13_hca":4881,"consumption_at_set_date_14_hca":5137,"consumption_at_set_date_15_hca":5393,"consumption_at_set_date_16_hca":5649,"consumption_at_set_date_17_hca":5905,"timestamp":"1111-11-11T11:11:11Z"}
// |HeatMeter;88018801;112233;MEASUREMENT RESET;1111-11-11 11:11.11


// Test: HeatMeter2 hcae2 60200770 NOKEY
// telegram=|76442D4870072060550872700720602D485508280060052F2F_0B6E320100426E550082016E3500C2016E1F0082026E1F00C2026E130082036E1300C2036E130082046E1300C2046E120082056E7D01C2056E440182066E0601C2066EB80082076E7F00C2076E320082086E1C00C2086E1C0002FD170000
// {"media":"heat cost allocation","meter":"hcae2","name":"HeatMeter2","id":"60200770","status":"OK","current_consumption_hca":132,"consumption_at_set_date_4_hca":31,"consumption_at_set_date_5_hca":19,"consumption_at_set_date_6_hca":19,"consumption_at_set_date_2_hca":53,"consumption_at_set_date_14_hca":127,"consumption_at_set_date_8_hca":19,"consumption_at_set_date_1_hca":85,"consumption_at_set_date_10_hca":381,"consumption_at_set_date_17_hca":28,"consumption_at_set_date_13_hca":184,"consumption_at_set_date_11_hca":324,"consumption_at_set_date_3_hca":31,"consumption_at_set_date_9_hca":18,"consumption_at_set_date_15_hca":50,"consumption_at_set_date_16_hca":28,"consumption_at_set_date_7_hca":19,"consumption_at_set_date_12_hca":262,"consumption_at_set_date_hca":85,"timestamp":"1111-11-11T11:11:11Z"}
// |HeatMeter2;60200770;132;OK;1111-11-11 11:11.11
