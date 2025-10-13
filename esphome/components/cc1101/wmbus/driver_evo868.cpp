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
        di.setName("evo868");
        di.setDefaultFields("name,id,total_m3,current_status,consumption_at_set_date_m3,set_date,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::T1);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
        di.addDetection(MANUFACTURER_MAD,  0x07,  0x50);

    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addStringFieldWithExtractorAndLookup(
            "current_status",
            "Status of meter.",
            DEFAULT_PRINT_PROPERTIES
            | PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ErrorFlags),
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::MapType::BitToString,
                        AlwaysTrigger, MaskBits(0xffff),
                        "OK",
                        {
                            // { 0x01 , "WOOT" },
                        }
                    },
                },
            });

        addOptionalLibraryFields("fabrication_no");
        addOptionalLibraryFields("total_m3");

        addNumericFieldWithExtractor(
            "consumption_at_set_date",
            "The total water consumption at the most recent billing period date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(1))
            );

        addStringFieldWithExtractor(
            "set_date",
            "The most recent billing period date.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "consumption_at_set_date_2",
            "The total water consumption at the second most recent billing period date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(2))
            );

        addStringFieldWithExtractor(
            "set_date_2",
            "The second most recent billing period date.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(2))
            );

        addNumericFieldWithExtractor(
            "max_flow_since_datetime",
            "Maximum water flow since date time.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Flow,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Maximum)
            .set(VIFRange::VolumeFlow)
            .set(StorageNr(3))
            );

        addStringFieldWithExtractor(
            "max_flow_datetime",
            "The datetime to which maximum flow is measured.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            .set(StorageNr(3))
            );

        addNumericFieldWithExtractor(
            "consumption_at_history_{storage_counter-7counter}",
            "The total water consumption at the historic date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(8),StorageNr(19))
            );

        addNumericFieldWithExtractor(
            "history_reference",
            "Reference date for history.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::PointInTime,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(8)),
            Unit::DateLT
            );

        addNumericFieldWithCalculatorAndMatcher(
            "history_{storage_counter-7counter}",
            "The historic date #.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::PointInTime,
            "history_reference_date - ((storage_counter-8counter) * 1 month)",
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(8),StorageNr(19)),
            Unit::DateLT
            );

        addStringFieldWithExtractor(
            "device_date_time",
            "Date and time when the meter sent the telegram.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            );
    }

}

// Test: Votchka evo868 79787776 NOKEY
// Comment: Test Maddalena EVO 868 wmbus module on water meter
// telegram=|aa4424347677787950077ac10000202f2f041306070000046d1e31b12104fd17000000000e787880048120004413c9040000426c9f2c840113c904000082016c9f2cd3013b9a0200c4016d0534a7218104fd280182046c9f2c840413c9040000c404131b00000084051300000000c405130000000084061300000000c406130000000084071300000000c407130000000084081300000000c408130000000084091300000000c4091300000000ffff|
// {"media":"water","meter":"evo868","name":"Votchka","id":"79787776","total_m3":1.798,"device_date_time":"2021-01-17 17:30","current_status":"OK","fabrication_no":"002081048078","consumption_at_set_date_m3":1.225,"set_date":"2020-12-31","consumption_at_set_date_2_m3":1.225,"set_date_2":"2020-12-31","max_flow_since_datetime_m3h":0.666,"max_flow_datetime":"2021-01-07 20:05","history_reference_date":"2020-12-31","consumption_at_history_1_m3":1.225,"history_1_date":"2020-12-31","consumption_at_history_2_m3":0.027,"history_2_date":"2020-11-30","consumption_at_history_3_m3":0,"history_3_date":"2020-10-31","consumption_at_history_4_m3":0,"history_4_date":"2020-09-30","consumption_at_history_5_m3":0,"history_5_date":"2020-08-31","consumption_at_history_6_m3":0,"history_6_date":"2020-07-31","consumption_at_history_7_m3":0,"history_7_date":"2020-06-30","consumption_at_history_8_m3":0,"history_8_date":"2020-05-31","consumption_at_history_9_m3":0,"history_9_date":"2020-04-30","consumption_at_history_10_m3":0,"history_10_date":"2020-03-31","consumption_at_history_11_m3":0,"history_11_date":"2020-02-29","consumption_at_history_12_m3":0,"history_12_date":"2020-01-31","timestamp":"1111-11-11T11:11:11Z"}
// |Votchka;79787776;1.798;OK;1.225;2020-12-31;1111-11-11 11:11.11
