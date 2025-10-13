/*
 Copyright (C) 2019-2022 Fredrik Öhrström (gpl-3.0-or-later)

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
        di.setName("aventieshca");
        di.setDefaultFields("name,id,current_consumption_hca,error_flags,timestamp");
        di.setMeterType(MeterType::HeatCostAllocationMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_AAA, 0x08,  0x55);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addStringFieldWithExtractorAndLookup(
            "status",
            "Meter status from error flags and tpl status field.",
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
                                { 0x01, "MEASUREMENT", TestBit::Set },
                                { 0x02, "SABOTAGE" },
                                { 0x04, "BATTERY" },
                                { 0x08, "CS" },
                                { 0x10, "HF" },
                                { 0x20, "RESET" }
                            }
                        },
                    },
                }));

        addStringFieldWithExtractorAndLookup(
            "error_flags",
            "Deprecated.",
            DEFAULT_PRINT_PROPERTIES | PrintProperty::DEPRECATED,
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
                            "",
                            {
                                { 0x01, "MEASUREMENT", TestBit::Set },
                                { 0x02, "SABOTAGE" },
                                { 0x04, "BATTERY" },
                                { 0x08, "CS" },
                                { 0x10, "HF" },
                                { 0x20, "RESET" }
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
            "consumption_at_set_date",
            "Heat cost allocation at the most recent billing period date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::HCA,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::HeatCostAllocation)
            .set(StorageNr(1))
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
            .set(StorageNr(2),StorageNr(17))
            );
    }
}

// Test: HCA aventieshca 60900126 NOKEY
// telegram=|76442104260190605508722601906021045508060060052F2F#0B6E660100426EA60082016EA600C2016E9E0082026E7E00C2026E5B0082036E4200C2036E770182046E5B01C2046E4C0182056E4701C2056E3E0182066E3B01C2066E3B0182076E3B01C2076E3B0182086E1301C2086E9C0002FD170000|
// {"media":"heat cost allocation","meter":"aventieshca","name":"HCA","id":"60900126","status":"OK","current_consumption_hca":166,"consumption_at_set_date_hca":166,"consumption_at_set_date_2_hca":166,"consumption_at_set_date_3_hca":158,"consumption_at_set_date_4_hca":126,"consumption_at_set_date_5_hca":91,"consumption_at_set_date_6_hca":66,"consumption_at_set_date_7_hca":375,"consumption_at_set_date_8_hca":347,"consumption_at_set_date_9_hca":332,"consumption_at_set_date_10_hca":327,"consumption_at_set_date_11_hca":318,"consumption_at_set_date_12_hca":315,"consumption_at_set_date_13_hca":315,"consumption_at_set_date_14_hca":315,"consumption_at_set_date_15_hca":315,"consumption_at_set_date_16_hca":275,"consumption_at_set_date_17_hca":156,"error_flags":"","timestamp":"1111-11-11T11:11:11Z"}
// |HCA;60900126;166;;1111-11-11 11:11.11
