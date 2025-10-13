/*
 Copyright (C) 2017-2023 Fredrik Öhrström (gpl-3.0-or-later)

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
        di.setName("aerius");
        di.setDefaultFields("name,id,total_m3,timestamp");
        di.setMeterType(MeterType::GasMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_DME, 0x03, 0x30);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addNumericFieldWithExtractor(
            "total",
            "The total gas consumption recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .add(VIFCombinable::ValueAtBaseCondC)
            );

        addNumericFieldWithExtractor(
            "flow",
            "The current gas flow.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Flow,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::VolumeFlow)
            );

        addNumericFieldWithExtractor(
            "temperature",
            "The current temperature.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::FlowTemperature)
            );

        addNumericFieldWithExtractor(
            "target",
            "Date time when previous billing period ended.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::PointInTime,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            .set(StorageNr(3))
            );

        addNumericFieldWithExtractor(
            "target",
            "The total gas consumption recorded when the previous billing period ended.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(3))
            .add(VIFCombinable::ValueAtBaseCondC)
            );

    }
}

// Test: aerius_gas aerius 99657098 NOKEY
// telegram=|2E44A5119870659930037A060020052F2F_0C933E842784060A3B00000A5A5901C4016D3B37DF2CCC01933E24032606|
// {"media":"gas","meter":"aerius","name":"aerius_gas","id":"99657098","total_m3":6842.784,"flow_m3h":0,"temperature_c":15.9,"target_datetime":"2022-12-31 23:59","target_m3":6260.324,"timestamp":"1111-11-11T11:11:11Z"}
// |aerius_gas;99657098;6842.784;1111-11-11 11:11.11
