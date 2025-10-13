/*
 Copyright (C) 2023 Fredrik Öhrström (gpl-3.0-or-later)

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
        di.setName("nemo");
        di.setDefaultFields(
            "name,id,status,"
            "total_active_positive_3phase_kwh,"
            "active_positive_3phase_kw,"
            "timestamp");
        di.setMeterType(MeterType::ElectricityMeter);
        di.addLinkMode(LinkMode::MBUS);
        di.addDetection(MANUFACTURER_IME,  0x02,  0x1d);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    // Telegram 1 /////////////////////////////////////////////////////////////////////

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addNumericFieldWithExtractor(
            "total_active_positive_3phase",
            "Et+ the total 3-phase active positive energy.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(StorageNr(0))
            .set(TariffNr(1))
            .set(SubUnitNr(1))
            );

        addNumericFieldWithExtractor(
            "active_positive_3phase",
            "P+ the 3-phase active positive power.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .set(StorageNr(0))
            .set(TariffNr(1))
            .set(SubUnitNr(1))
            );

        addNumericFieldWithExtractor(
            "total_reactive_positive_3phase",
            "Er+ the total 3-phase reactive positive energy.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(StorageNr(0))
            .set(TariffNr(1))
            .set(SubUnitNr(2))
            );

        addNumericFieldWithExtractor(
            "reactive_positive_3phase",
            "Q+ the 3-phase reactive positive power.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .set(StorageNr(0))
            .set(TariffNr(1))
            .set(SubUnitNr(2))
            );

        addNumericFieldWithExtractor(
            "total_active_partial_3phase",
            "Part Et+ the total 3-phase active partial energy.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(StorageNr(0))
            .set(TariffNr(2))
            .set(SubUnitNr(1))
            );

        addNumericFieldWithExtractor(
            "active_negative_3phase",
            "P- the 3-phase active negative power.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .set(StorageNr(0))
            .set(TariffNr(2))
            .set(SubUnitNr(1))
            );

        addNumericFieldWithExtractor(
            "total_reactive_partial_3phase",
            "Part Er+ the total 3-phase reactive partial energy.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(StorageNr(0))
            .set(TariffNr(2))
            .set(SubUnitNr(2))
            );

        addNumericFieldWithExtractor(
            "reactive_negative_3phase",
            "Q- the 3-phase reactive negative power.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .set(StorageNr(0))
            .set(TariffNr(2))
            .set(SubUnitNr(2))
            );

        addNumericFieldWithExtractor(
            "power",
            "PF the power factor.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Dimensionless),
            Unit::FACTOR
            );

        addStringFieldWithExtractorAndLookup(
            "status",
            "Status. OK if no error flags are set.",
            PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ErrorFlags),
            Translate::Lookup()
            .add(Translate::Rule("ERROR_FLAGS", Translate::MapType::BitToString)
                 .set(MaskBits(0xff))
                 .set(DefaultMessage("OK"))
                ));

        // Telegram 2 /////////////////////////////////////////////////////////////

        addNumericFieldWithExtractor(
            "current_at_phase_1",
            "I1 Amperage for L1 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Amperage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Amperage)
            .add(VIFCombinable::Mfct01)
            );

        addNumericFieldWithExtractor(
            "current_at_phase_2",
            "I2 Amperage for L2 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Amperage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Amperage)
            .add(VIFCombinable::Mfct02)
            );

        addNumericFieldWithExtractor(
            "current_at_phase_3",
            "I3 Amperage for L3 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Amperage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Amperage)
            .add(VIFCombinable::Mfct03)
            );

        addNumericFieldWithExtractor(
            "voltage_at_phase_1",
            "L1-N Voltage for L1 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Voltage)
            .add(VIFCombinable::Mfct01)
            );

        addNumericFieldWithExtractor(
            "voltage_at_phase_2",
            "L2-N Voltagefor L2 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Voltage)
            .add(VIFCombinable::Mfct02)
            );

        addNumericFieldWithExtractor(
            "voltage_at_phase_3",
            "L3-N Voltage for L3 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Voltage)
            .add(VIFCombinable::Mfct03)
            );

        // Telegram 3 /////////////////////////////////////////////////////////////

        addNumericFieldWithExtractor(
            "active_power_at_phase_1",
            "P1 Power for L1 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .set(SubUnitNr(1))
            .add(VIFCombinable::Mfct01)
            );

        addNumericFieldWithExtractor(
            "active_power_at_phase_2",
            "P2 Power for L2 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .set(SubUnitNr(1))
            .add(VIFCombinable::Mfct02)
            );

        addNumericFieldWithExtractor(
            "active_power_at_phase_3",
            "P3 Power for L3 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .set(SubUnitNr(1))
            .add(VIFCombinable::Mfct03)
            );

        addNumericFieldWithExtractor(
            "reactive_power_at_phase_1",
            "Q1 Power for L1 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .set(SubUnitNr(2))
            .add(VIFCombinable::Mfct01)
            );

        addNumericFieldWithExtractor(
            "reactive_power_at_phase_2",
            "Q2 Power for L2 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .set(SubUnitNr(2))
            .add(VIFCombinable::Mfct02)
            );

        addNumericFieldWithExtractor(
            "reactive_power_at_phase_3",
            "Q3 Power for L3 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .set(SubUnitNr(2))
            .add(VIFCombinable::Mfct03)
            );

        addNumericFieldWithExtractor(
            "at_phase_1_power",
            "PF1 the power factor for L1 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Dimensionless)
            .add(VIFCombinable::Mfct01),
            Unit::FACTOR
            );

        addNumericFieldWithExtractor(
            "at_phase_2_power",
            "PF2 the power factor for L2 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Dimensionless)
            .add(VIFCombinable::Mfct02),
            Unit::FACTOR
            );

        addNumericFieldWithExtractor(
            "at_phase_3_power",
            "PF3 the power factor for L3 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Dimensionless)
            .add(VIFCombinable::Mfct03),
            Unit::FACTOR
            );

        addNumericFieldWithExtractor(
            "voltage_l1_l2",
            "L1-L2 Voltage between phases.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Voltage)
            .add(VIFCombinable::Mfct04)
            );

        addNumericFieldWithExtractor(
            "voltage_l2_l3",
            "L2-L3 Voltage between phases.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Voltage)
            .add(VIFCombinable::Mfct05)
            );

        addNumericFieldWithExtractor(
            "voltage_l3_l1",
            "L3-L1 Voltage between phases.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Voltage)
            .add(VIFCombinable::Mfct06)
            );

        addNumericFieldWithExtractor(
            "current_in_neutral",
            "I Neutral amperage.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Amperage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Amperage)
            .add(VIFCombinable::Mfct04)
            );


        addNumericFieldWithExtractor(
            "raw_frequency",
            "Frequency in 0.1 Hz",
            DEFAULT_PRINT_PROPERTIES  | PrintProperty::HIDE,
            Quantity::Frequency,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("05FF5A"))
            );

        addNumericFieldWithCalculator(
            "frequency",
            "Frequency of AC.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Frequency,
            "raw_frequency_hz / 10 counter");


    }
}

// Test: Elen nemo 00067609 NOKEY
// comment: Telegram 1
// telegram=|6864646808657209760600A5251D02000000008E500400355873060085502B0013BF478E9040040029452501008590402B00F800468E600400000000000085602B000000008EA0400400890000000085A0402B0000000005FD3ADCF97E3F01FD17001F00000000009116|
// {"media":"electricity", "meter":"nemo", "name":"Elen", "id":"00067609", "total_active_positive_3phase_kwh":6735835, "active_positive_3phase_kw":97.83, "total_reactive_positive_3phase_kwh":1254529, "reactive_positive_3phase_kw":8.254, "total_active_partial_3phase_kwh":0, "active_negative_3phase_kw":0, "total_reactive_partial_3phase_kwh":89, "reactive_negative_3phase_kw":0, "power_factor":-0.996, "status":"OK", "frequency_hz":null, "timestamp":"1111-11-11T11:11:11Z"}
// |Elen;00067609;OK;6735835;97.83;1111-11-11 11:11.11

// comment: Telegram 2
// telegram=|684B4B6808657209760600A5251D020100000005FDD9FF0100F4174805FDD9FF0200B8084805FDD9FF0300B0014805FDC8FF0100C0104505FDC8FF020040114505FDC8FF03005011451F00000000000716|
// {"media":"electricity", "meter":"nemo", "name":"Elen", "id":"00067609", "total_active_positive_3phase_kwh":6735835, "active_positive_3phase_kw":97.83, "total_reactive_positive_3phase_kwh":1254529, "reactive_positive_3phase_kw":8.254, "total_active_partial_3phase_kwh":0, "active_negative_3phase_kw":0, "total_reactive_partial_3phase_kwh":89, "reactive_negative_3phase_kw":0, "power_factor":-0.996, "status":"OK", "current_at_phase_1_a":155.6, "current_at_phase_2_a":140, "current_at_phase_3_a":132.8, "voltage_at_phase_1_v":231.6, "voltage_at_phase_2_v":232.4, "voltage_at_phase_3_v":232.5, "frequency_hz":null, "timestamp":"1111-11-11T11:11:11Z"}
// |Elen;00067609;OK;6735835;97.83;1111-11-11 11:11.11

// comment: Telegram 3
// telegram=|689E9E6808657209760600A5251D02020000008540ABFF0100360B478540ABFF02002CFA468540ABFF030074ED46858040ABFF0100C0E244858040ABFF0200405A45858040ABFF030060364505FDBAFF0178BE7F3F05FDBAFF0240357E3F05FDBAFF0353B87E3F05FDC8FF0400907A4505FDC8FF0500707B4505FDC8FF0600807B4505FDD9FF0400502A4705FF5A0000FA4302FD3AC80002FD3A0A000F00000000008B16|
// {"media":"electricity", "meter":"nemo", "name":"Elen", "id":"00067609", "total_active_positive_3phase_kwh":6735835, "active_positive_3phase_kw":97.83, "total_reactive_positive_3phase_kwh":1254529, "reactive_positive_3phase_kw":8.254, "total_active_partial_3phase_kwh":0, "active_negative_3phase_kw":0, "total_reactive_partial_3phase_kwh":89, "reactive_negative_3phase_kw":0, "power_factor":-200, "status":"OK", "current_at_phase_1_a":155.6, "current_at_phase_2_a":140, "current_at_phase_3_a":132.8, "voltage_at_phase_1_v":231.6, "voltage_at_phase_2_v":232.4, "voltage_at_phase_3_v":232.5, "active_power_at_phase_1_kw":35.638, "active_power_at_phase_2_kw":32.022, "active_power_at_phase_3_kw":30.394, "reactive_power_at_phase_1_kw":1.814, "reactive_power_at_phase_2_kw":3.492, "reactive_power_at_phase_3_kw":2.918, "at_phase_1_power_factor":-0.999, "at_phase_2_power_factor":-0.993, "at_phase_3_power_factor":-0.995, "voltage_l1_l2_v":400.9, "voltage_l2_l3_v":402.3, "voltage_l3_l1_v":402.4, "current_in_neutral_a":43.6, "frequency_hz":50, "timestamp":"1111-11-11T11:11:11Z"}
// |Elen;00067609;OK;6735835;97.83;1111-11-11 11:11.11
