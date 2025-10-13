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
        di.setName("esyswm");
        di.setDefaultFields("name,id,total_energy_consumption_kwh,current_power_consumption_kw,total_energy_production_kwh,total_energy_consumption_tariff1_kwh,total_energy_consumption_tariff2_kwh,current_power_consumption_phase1_kw,current_power_consumption_phase2_kw,current_power_consumption_phase3_kw,enhanced_id,timestamp");
        di.setMeterType(MeterType::ElectricityMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_ESY, 0x37, 0x30); // Wireless adapter for electricity meter.
        di.addDetection(MANUFACTURER_ESY, 0x02, 0x11);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addOptionalLibraryFields("fabrication_no,enhanced_id,location");

        addStringFieldWithExtractor(
            "location_hex",
            "Meter installed at this customer location. Deprecate field.",
            DEFAULT_PRINT_PROPERTIES  | PrintProperty::DEPRECATED,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Location)
            );

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
            "current_power_consumption",
            "Calculated sum of power consumption of all phases.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            );

        addNumericFieldWithExtractor(
            "total_energy_production",
            "The total energy production recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .add(VIFCombinable::BackwardFlow)
            );

        addNumericFieldWithExtractor(
            "total_energy_consumption_tariff1",
             "The total energy consumption recorded by this meter on tariff 1.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(TariffNr(1))
            );

        addNumericFieldWithExtractor(
            "total_energy_consumption_tariff2",
             "The total energy consumption recorded by this meter on tariff 2.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(TariffNr(2))
            );

        addNumericFieldWithExtractor(
            "current_power_consumption_phase1",
            "Current power consumption phase 1.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("04A9FF01"))
            );

        addNumericFieldWithExtractor(
            "current_power_consumption_phase2",
            "Current power consumption phase 2.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("04A9FF02"))
            );

        addNumericFieldWithExtractor(
            "current_power_consumption_phase3",
            "Current power consumption phase 3.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("04A9FF03"))
            );

        addStringFieldWithExtractor(
            "version",
            "Static version information.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(DifVifKey("0DFD09"))
            );

    }
}

// Test: Elen2 esyswm 77997799 NOKEY
// Comment: Static telegram
// telegram=|7B4479169977997730378C208B900F002C25E4EF0A002EA98E7D58B3ADC57299779977991611028B005087102F2F#0DFD090F34302e3030562030303030303030300D790E31323334353637383839595345310DFD100AAAAAAAAAAAAAAAAAAAAA0D780E31323334353637383930594553312F2F2F2F2F2F2F2F2F2F2F|
// {"media":"electricity","meter":"esyswm","name":"Elen2","id":"77997799","fabrication_no":"1SEY0987654321","enhanced_id":"1ESY9887654321","location":"AAAAAAAAAAAAAAAAAAAA","location_hex":"AAAAAAAAAAAAAAAAAAAA","version":"00000000 V00.04","timestamp":"1111-11-11T11:11:11Z"}
// |Elen2;77997799;null;null;null;null;null;null;null;null;1ESY9887654321;1111-11-11 11:11.11

// Comment: Dynamic telegram
// telegram=|7B4479169977997730378C20F0900F002C2549EE0A0077C19D3D1A08ABCD729977997779161102F0005007102F2F#0702F5C3FA000000000007823C5407000000000000841004E081020084200415000000042938AB000004A9FF01FA0A000004A9FF02050A000004A9FF03389600002F2F2F2F2F2F2F2F2F2F2F2F2F|
// {"media":"electricity","meter":"esyswm","name":"Elen2","id":"77997799","fabrication_no":"1SEY0987654321","enhanced_id":"1ESY9887654321","location":"AAAAAAAAAAAAAAAAAAAA","location_hex":"AAAAAAAAAAAAAAAAAAAA","total_energy_consumption_kwh":1643.4165,"current_power_consumption_kw":0.43832,"total_energy_production_kwh":0.1876,"total_energy_consumption_tariff1_kwh":1643.2,"total_energy_consumption_tariff2_kwh":0.21,"current_power_consumption_phase1_kw":0.0281,"current_power_consumption_phase2_kw":0.02565,"current_power_consumption_phase3_kw":0.38456,"version":"00000000 V00.04","timestamp":"1111-11-11T11:11:11Z"}
// |Elen2;77997799;1643.4165;0.43832;0.1876;1643.2;0.21;0.0281;0.02565;0.38456;1ESY9887654321;1111-11-11 11:11.11
