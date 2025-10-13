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
        di.setName("em24");
        di.setDefaultFields(
            "name,id,"
            "total_energy_consumption_kwh,total_energy_production_kwh,"
            "total_reactive_energy_consumption_kvarh,total_reactive_energy_production_kvarh,"
            "total_apparent_energy_consumption_kvah,total_apparent_energy_production_kvah,"
            "timestamp");
        di.setMeterType(MeterType::ElectricityMeter);
        di.addLinkMode(LinkMode::C1);
        di.addDetection(MANUFACTURER_KAM,  0x02,  0x33);
        di.addDetection(MANUFACTURER_GAV,  0x02,  0x00);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addStringFieldWithExtractorAndLookup(
            "status",
            "Status of meter.",
            DEFAULT_PRINT_PROPERTIES | PrintProperty::STATUS,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ErrorFlags),
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::MapType::BitToString,
                        AlwaysTrigger, MaskBits(0xff),
                        "OK",
                        {
                            { 0x01, "V_1_OVERFLOW" },
                            { 0x02, "V_2_OVERFLOW" },
                            { 0x04, "V_2_OVERFLOW" },
                            { 0x08, "I_1_OVERFLOW" },
                            { 0x10, "I_2_OVERFLOW" },
                            { 0x20, "I_3_OVERFLOW" },
                            { 0x40, "FREQUENCY" },
                        }
                    },
                },
            });

        addStringFieldWithExtractorAndLookup(
            "error",
            "Any errors currently being reported, this field is deprecated and replaced by the status field.",
            DEFAULT_PRINT_PROPERTIES | PrintProperty::DEPRECATED,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ErrorFlags),
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::MapType::BitToString,
                        AlwaysTrigger, MaskBits(0xff),
                        "",
                        {
                            { 0x01, "V_1_OVERFLOW" },
                            { 0x02, "V_2_OVERFLOW" },
                            { 0x04, "V_2_OVERFLOW" },
                            { 0x08, "I_1_OVERFLOW" },
                            { 0x10, "I_2_OVERFLOW" },
                            { 0x20, "I_3_OVERFLOW" },
                            { 0x40, "FREQUENCY" },
                        }
                    },
                },
            });

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
            "power",
            "Power measured by this meter at the moment.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            );

        addNumericFieldWithExtractor(
            "total_energy_production",
            "The total energy backward (production) recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .add(VIFCombinable::BackwardFlow)
            );

        addNumericFieldWithExtractor(
            "total_reactive_energy_consumption",
            "The reactive total energy consumption recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Reactive_Energy,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("04FB8275"))
            );

        addNumericFieldWithExtractor(
            "total_reactive_energy_production",
            "The total reactive energy backward (production) recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Reactive_Energy,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(DifVifKey("04FB82F53C"))
            );

        addNumericFieldWithCalculator(
            "total_apparent_energy_consumption",
            "Calculated: the total apparent energy consumption.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Apparent_Energy,
            "sqrt("
            "      (total_energy_consumption_kwh * total_energy_consumption_kwh) "
            "      +"
            "      (total_reactive_energy_consumption_kvarh * total_reactive_energy_consumption_kvarh)"
            "    )");

        addNumericFieldWithCalculator(
            "total_apparent_energy_production",
            "Calculated: the total apparent energy production.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Apparent_Energy,
            R"STR(
                     sqrt( (total_energy_production_kwh * total_energy_production_kwh) +
                           (total_reactive_energy_production_kvarh * total_reactive_energy_production_kvarh) )
            )STR");

        addNumericFieldWithExtractor(
            "amperage_at_phase_1",
            "Amperage at phase L1.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Amperage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("04FDD9FC01"))
            );

        addNumericFieldWithExtractor(
            "amperage_at_phase_2",
            "Amperage at phase L2.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Amperage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("04FDD9FC02"))
            );

        addNumericFieldWithExtractor(
            "amperage_at_phase_3",
            "Amperage at phase L3.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Amperage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("04FDD9FC03"))
            );

        addNumericFieldWithExtractor(
            "voltage_at_phase_1",
            "Voltage at phase L1.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("04FDC8FC01"))
            );

        addNumericFieldWithExtractor(
            "voltage_at_phase_2",
            "Voltage at phase L2.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("04FDC8FC02"))
            );

        addNumericFieldWithExtractor(
            "voltage_at_phase_3",
            "Voltage at phase L3.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("04FDC8FC03"))
            );

        addNumericFieldWithExtractor(
            "raw_frequency",
            "Frequency in 0.1 Hz",
            DEFAULT_PRINT_PROPERTIES  | PrintProperty::HIDE,
            Quantity::Frequency,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("02FB2E"))
            );

        addNumericFieldWithCalculator(
            "frequency",
            "Frequency of AC.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Frequency,
            "raw_frequency_hz / 10 counter");
    }
}

// Test: Elen em24 66666666 NOKEY
// telegram=|35442D2C6666666633028D2070806A0520B4D378_0405F208000004FB82753F00000004853C0000000004FB82F53CCA01000001FD1722|
// {"media":"electricity","meter":"em24","name":"Elen","id":"66666666","status":"I_3_OVERFLOW V_2_OVERFLOW","error":"I_3_OVERFLOW V_2_OVERFLOW","total_energy_consumption_kwh":229,"total_energy_production_kwh":0,"total_reactive_energy_consumption_kvarh":63,"total_reactive_energy_production_kvarh":458,"total_apparent_energy_consumption_kvah":237.507895,"total_apparent_energy_production_kvah":458,"frequency_hz":null,"timestamp":"1111-11-11T11:11:11Z"}
// |Elen;66666666;229;0;63;458;237.507895;458;1111-11-11 11:11.11

// Test: Elen2 em24 02020202 NOKEY
// telegram=|4144361C0202020200028C209A7A9A0030252F2F_04050100000004FB82750000000004FB82F53C00000000042A0000000001FD17002F2F2F2F2F2F2F2F2F2F2F2F2F|
// {"error": "","id": "02020202","media": "electricity","meter": "em24","name": "Elen2","power_kw": 0,"status": "OK","timestamp": "1111-11-11T11:11:11Z","total_apparent_energy_consumption_kvah": 0.1,"total_apparent_energy_production_kvah": null,"total_energy_consumption_kwh": 0.1,"total_reactive_energy_consumption_kvarh": 0,"total_reactive_energy_production_kvarh": 0,"frequency_hz":null}
// |Elen2;02020202;0.1;null;0;0;0.1;null;1111-11-11 11:11.11


// telegram=|8144361C0202020200028C20357A351070252F2F_04050100000004FB82750000000004FB82F53C00000000042A0000000004FB140000000004FB943C0000000004FDD9FC010000000004FDD9FC020000000004FDD9FC030000000004FDC8FC011709000004FDC8FC02EC04000004FDC8FC03EC04000002FB2EF40101FD17002F2F2F|
// {"amperage_at_phase_1_a": 0,"amperage_at_phase_2_a": 0,"amperage_at_phase_3_a": 0,"error": "","frequency_hz": 50,"id": "02020202","media": "electricity","meter": "em24","name": "Elen2","power_kw": 0,"status": "OK","timestamp": "1111-11-11T11:11:11Z","total_apparent_energy_consumption_kvah": 0.1,"total_apparent_energy_production_kvah": null,"total_energy_consumption_kwh": 0.1,"total_reactive_energy_consumption_kvarh": 0,"total_reactive_energy_production_kvarh": 0,"voltage_at_phase_1_v": 232.7,"voltage_at_phase_2_v": 126,"voltage_at_phase_3_v": 126}
// |Elen2;02020202;0.1;null;0;0;0.1;null;1111-11-11 11:11.11
