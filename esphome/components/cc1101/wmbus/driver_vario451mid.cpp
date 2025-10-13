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
        di.setName("vario451mid");
        di.setDefaultFields("name,id,total_energy_consumption_kwh,energy_at_old_date_kwh,energy_at_set_date_kwh,timestamp");
        di.setMeterType(MeterType::HeatMeter);
        // Mode 7
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_TCH, 0x04,  0x17);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) :  MeterCommonImplementation(mi, di)
    {
        addNumericFieldWithExtractor(
            "total_energy_consumption",
            "The total energy consumption recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            );

        addNumericFieldWithExtractor(
            "energy_at_old_date",
            "The total energy consumption recorded when?",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "old",
            "The last billing old date?",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::PointInTime,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1)),
            Unit::DateLT);

        addNumericFieldWithExtractor(
            "energy_at_set_date",
            "The total energy consumption recorded by this meter at the due date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(StorageNr(8))
            );

        addNumericFieldWithExtractor(
            "set",
            "The last billing set date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::PointInTime,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(8)),
            Unit::DateLT);
    }
}

// Test: Heato vario451mid 94430412 NOKEY
// telegram=734468501204439417048c0084900f002c2536700000B767B64527c50ac67a33005007102f2f8404062846000082046c9f2c8d04861f1e72fe00000000000000000000000000000000000000000000000000000000440600000000426cffff0406c94700002f2f2f2f2f2f2f2f2f2f2f2f2f2f2f
// {"media":"heat","meter":"vario451mid","name":"Heato","id":"94430412","total_energy_consumption_kwh":18377,"energy_at_old_date_kwh":0,"old_date":"2128-03-31","energy_at_set_date_kwh":17960,"set_date":"2020-12-31","timestamp":"1111-11-11T11:11:11Z"}
// |Heato;94430412;18377;0;17960;1111-11-11 11:11.11
