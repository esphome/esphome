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
        di.setName("q400");
        di.setDefaultFields("name,id,total_m3,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_AXI, 0x07,  0x10);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addOptionalLibraryFields("meter_datetime,on_time_h");
        addOptionalLibraryFields("total_m3,total_forward_m3,total_backward_m3,flow_temperature_c,volume_flow_m3h");

        addStringField(
            "status",
            "Status and error flags.",
            DEFAULT_PRINT_PROPERTIES | PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS);

        addStringFieldWithExtractor(
            "set_datetime",
            "Date and time when the previous billing period ended.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "consumption_at_set_date",
            "The total water consumption at the end of the previous billing period.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "forward_at_set_date",
            "The total media volume flowing forward at the end of previous billing period.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(1))
            .add(VIFCombinable::ForwardFlow)
            );

        addNumericFieldWithExtractor(
            "backward_at_set_date",
            "The total media volume flowing backward at the end of the previous billing period.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(1))
            .add(VIFCombinable::BackwardFlow)
            );

        addNumericFieldWithExtractor(
            "battery",
            "Percentage of battery remaining.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("01FD74")),
            Unit::PERCENTAGE
            );
    }
}

// Test: Q400Water q400 72727272 NOKEY
// telegram=|2E4409077272727210077AD71020052F2F_046D040D742C041377000000446D0000612C4413000000002F2F2F2F2F2F|
// {"media":"water","meter":"q400","name":"Q400Water","id":"72727272","meter_datetime":"2019-12-20 13:04","total_m3":0.119,"status":"TEMPORARY_ERROR","set_datetime":"2019-12-01 00:00","consumption_at_set_date_m3":0,"timestamp":"1111-11-11T11:11:11Z"}
// |Q400Water;72727272;0.119;1111-11-11 11:11.11

// Test: AxiomaWater q400 72727273 NOKEY
// Comment: Test Axioma W1 telegram with additional fields compared to the older q400 meter.
// telegram=|5E4409077372727210077A710050052F2F_046D0110A92704130022000004933B0000000004933C00000000023B000002592A0A446D0000A12744130000000044933B0000000044933C0000000001FD74622F2F2F2F2F2F2F2F2F2F2F2F2F2F|
// {"media":"water","meter":"q400","name":"AxiomaWater","id":"72727273","meter_datetime":"2021-07-09 16:01","total_m3":8.704,"total_forward_m3":0,"total_backward_m3":0,"flow_temperature_c":26.02,"volume_flow_m3h":0,"status":"OK","set_datetime":"2021-07-01 00:00","consumption_at_set_date_m3":0,"forward_at_set_date_m3":0,"backward_at_set_date_m3":0,"battery_pct":98,"timestamp":"1111-11-11T11:11:11Z"}
// |AxiomaWater;72727273;8.704;1111-11-11 11:11.11

// Test: M q400 05829163 NOKEY
// telegram=|544409076391820510077ABF100000046D2A0DC62C0420E80F430104130000000004933B0000000004933C00000000023B00000259F0D8446D0000C12C44130000000044933B0000000044933C0000000001FD7461|
// {"backward_at_set_date_m3": 0,"battery_pct": 97,"consumption_at_set_date_m3": 0,"flow_temperature_c": -100,"forward_at_set_date_m3": 0,"id": "05829163","media": "water","meter": "q400","meter_datetime": "2022-12-06 13:42","name": "M","on_time_h": 5881.166667,"set_datetime": "2022-12-01 00:00","status": "TEMPORARY_ERROR","timestamp": "1111-11-11T11:11:11Z","total_backward_m3": 0,"total_forward_m3": 0,"total_m3": 0,"volume_flow_m3h": 0}
// |M;05829163;0;1111-11-11 11:11.11
