/*
 Copyright (C) 2022 Fredrik Öhrström (gpl-3.0-or-later)

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
        di.setName("microclima");
        di.setDefaultFields("name,id,status,total_energy_consumption_kwh,total_volume_m3,timestamp");
        di.setMeterType(MeterType::HeatMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_MAD, 0x04, 0x00);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addOptionalLibraryFields("meter_datetime,model_version,parameter_set");
        addOptionalLibraryFields("flow_temperature_c,return_temperature_c");

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
                            }
                        },
                    },
                }));

        addNumericFieldWithExtractor(
            "total_energy_consumption",
            "The total heat energy consumption recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            );

        addNumericFieldWithExtractor(
            "total_volume",
            "The total heating media volume recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            );

        addNumericFieldWithExtractor(
            "volume_flow",
            "The current heat media volume flow.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Flow,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::VolumeFlow)
            );

        addNumericFieldWithExtractor(
            "power",
            "The current power consumption.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::PowerW)
            );

        addNumericFieldWithExtractor(
            "temperature_difference",
            "The difference between flow and return media temperatures.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::TemperatureDifference)
            );

        addNumericFieldWithExtractor(
            "set",
            "The most recent billing period date.",
            DEFAULT_PRINT_PROPERTIES  | PrintProperty::HIDE,
            Quantity::PointInTime,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1)),
            Unit::DateLT
            );

        addNumericFieldWithExtractor(
            "consumption_at_set_date_{storage_counter}",
            "The total water consumption at the historic date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(StorageNr(1),StorageNr(31))
            );

        addNumericFieldWithCalculatorAndMatcher(
            "set_date_{storage_counter}",
            "Unclear! What is the date really?",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::PointInTime,
            "set_date - ((storage_counter-1counter) * 1 month)",
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(StorageNr(1),StorageNr(31)),
            Unit::DateLT
            );

    }
}

// The meter sends two types of telegrams a shorter with current values.

// Test: Heat microclima 93572431 NOKEY
// telegram=|494424343124579300047a5a0000202f2f046d2720b62c04060d07000001fd170004130a8c0400043b00000000042b00000000025b1500025f15000261d0ff03fd0c05000002fd0b1011|
// {"flow_temperature_c":21,"id":"93572431","media":"heat","meter":"microclima","meter_datetime":"2021-12-22 00:39","model_version":"000005","name":"Heat","parameter_set":"1110","power_kw":0,"return_temperature_c":21,"status":"OK","temperature_difference_c":-0.48,"timestamp":"1111-11-11T11:11:11Z","total_energy_consumption_kwh":1805,"total_volume_m3":297.994,"volume_flow_m3h":0}
// |Heat;93572431;OK;1805;297.994;1111-11-11 11:11.11


// And a longer with historical values. This telegram is not yet properly decoded.

// Test: Heat microclima 93573086 NOKEY
// telegram=|A44424348630579300047ADD0000202F2F046D0721B62C04064708000004135DB0030001FD1700426C9F2C4406C6040000C40106C0070000C4020637070000C4030611070000C404060B070000C405060B070000C406060B070000C407060B070000C40806A5060000C40906F7050000C40A067A050000C40B060F050000C40C06C6040000C40D063F040000C40E06BB030000C40F06A502000003FD0C05000002FD0B1111|
// {"consumption_at_set_date_11_kwh":1803,"consumption_at_set_date_13_kwh":1803,"consumption_at_set_date_15_kwh":1803,"consumption_at_set_date_17_kwh":1701,"consumption_at_set_date_19_kwh":1527,"consumption_at_set_date_1_kwh":1222,"consumption_at_set_date_21_kwh":1402,"consumption_at_set_date_23_kwh":1295,"consumption_at_set_date_25_kwh":1222,"consumption_at_set_date_27_kwh":1087,"consumption_at_set_date_29_kwh":955,"consumption_at_set_date_31_kwh":677,"consumption_at_set_date_3_kwh":1984,"consumption_at_set_date_5_kwh":1847,"consumption_at_set_date_7_kwh":1809,"consumption_at_set_date_9_kwh":1803,"id":"93573086","media":"heat","meter":"microclima","meter_datetime":"2021-12-22 01:07","model_version":"000005","name":"Heat","parameter_set":"1111","set_date_11_date":"2020-02-29","set_date_13_date":"2019-12-31","set_date_15_date":"2019-10-31","set_date_17_date":"2019-08-31","set_date_19_date":"2019-06-30","set_date_1_date":"2020-12-31","set_date_21_date":"2019-04-30","set_date_23_date":"2019-02-28","set_date_25_date":"2018-12-31","set_date_27_date":"2018-10-31","set_date_29_date":"2018-08-31","set_date_31_date":"2018-06-30","set_date_3_date":"2020-10-31","set_date_5_date":"2020-08-31","set_date_7_date":"2020-06-30","set_date_9_date":"2020-04-30","status":"OK","timestamp":"1111-11-11T11:11:11Z","total_energy_consumption_kwh":2119,"total_volume_m3":241.757}
// |Heat;93573086;OK;2119;241.757;1111-11-11 11:11.11
