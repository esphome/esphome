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
        di.setName("sontex868");
        di.setDefaultFields("name,id,current_consumption_hca,set_date,consumption_at_set_date_hca,timestamp");

        di.setMeterType(MeterType::HeatCostAllocationMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_SON, 0x08,  0x16);

        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addNumericFieldWithExtractor(
            "current_consumption",
            "The current heat cost allocation for this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::HCA,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::HeatCostAllocation)
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
            "current_temp",
            "The current temperature of the heating element.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::FlowTemperature)
            );

        addNumericFieldWithExtractor(
            "current_room_temp",
            "The current room temperature.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ExternalTemperature)
            );

        addNumericFieldWithExtractor(
            "max_temp",
            "The maximum temperature so far during this billing period.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Maximum)
            .set(VIFRange::FlowTemperature)
            );


        addNumericFieldWithExtractor(
            "max_temp_previous_period",
             "The maximum temperature during the previous billing period.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Maximum)
            .set(VIFRange::FlowTemperature)
            .set(StorageNr(1))
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

// Test: MyHeatCoster sontex868 27282728 NOKEY
// telegram=|AF46EE4D2827282716087A80000000_046D040A9F2A036E770000426CE1F7436E660000525900008288016C61258388016E0000008D8801EE1E3533FE00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005FF2D0000803F8520FF2D0000803F0259AD0A0265D8041259AD0A8310FD3100000082106C01018110FD610082206C9F2A0BFD0F01030102FF2C000002FD66AC08|
// {"media":"heat cost allocation","meter":"sontex868","name":"MyHeatCoster","id":"27282728","current_consumption_hca":119,"set_date":"2127-07-01","consumption_at_set_date_hca":102,"current_temp_c":27.33,"current_room_temp_c":12.4,"max_temp_c":27.33,"max_temp_previous_period_c":0,"device_date_time":"2020-10-31 10:04","timestamp":"1111-11-11T11:11:11Z"}
// |MyHeatCoster;27282728;119;2127-07-01;102;1111-11-11 11:11.11
