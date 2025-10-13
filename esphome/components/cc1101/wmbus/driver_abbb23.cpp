/*
 Copyright (C) 2019-2023 Fredrik Öhrström (gpl-3.0-or-later)

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
        di.setName("abbb23");
        di.setDefaultFields("name,id,total_energy_consumption_kwh,timestamp");
        di.setMeterType(MeterType::ElectricityMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_ABB,  0x02,  0x20);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
      addStringField(
            "status",
            "Status, error, warning and alarm flags.",
            DEFAULT_PRINT_PROPERTIES | PrintProperty::INCLUDE_TPL_STATUS | PrintProperty::STATUS);

        addNumericFieldWithExtractor(
            "total_energy_consumption",
            "Total cumulative active imported energy.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "total_energy_consumption_tariff_{tariff_counter}",
            "Total cumulative active imported energy per tariff.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(TariffNr(1),TariffNr(4))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "total_energy_production",
            "Total cumulative active exported energy.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(TariffNr(0))
            .set(SubUnitNr(1))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "total_energy_production_tariff_{tariff_counter}",
            "Total cumulative active exported energy per tariff.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(TariffNr(1),TariffNr(4))
            .set(SubUnitNr(1))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "active_tariff",
            "Active tariff.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("01FF9300")),
            Unit::NUMBER
            );

        addNumericFieldWithExtractor(
            "ct_numerator",
            "Current transformer ratio (numerator).",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("04FFA015")),
            Unit::FACTOR
            );

        addNumericFieldWithExtractor(
            "vt_numerator",
            "Voltage transformer ratio (numerator).",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("04FFA115")),
            Unit::FACTOR
            );

        addNumericFieldWithExtractor(
            "ct_denominator",
            "Current transformer ratio (denominator).",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("04FFA215")),
            Unit::FACTOR
            );

        addNumericFieldWithExtractor(
            "vt_denominator",
            "Voltage transformer ratio (denominator).",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("04FFA315")),
            Unit::FACTOR
            );

       addStringFieldWithExtractorAndLookup(
            "error_flags",
            "Error flags.",
            DEFAULT_PRINT_PROPERTIES | PrintProperty::INJECT_INTO_STATUS,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(DifVifKey("07FFA600")),
            Translate::Lookup()
            .add(Translate::Rule("ERROR_FLAGS", Translate::MapType::BitToString)
                 .set(MaskBits(0xffffffffffffffff))
                 .set(DefaultMessage("OK"))
                ));

       addStringFieldWithExtractorAndLookup(
            "warning_flags",
            "Warning flags.",
            DEFAULT_PRINT_PROPERTIES | PrintProperty::INJECT_INTO_STATUS,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(DifVifKey("07FFA700")),
            Translate::Lookup()
            .add(Translate::Rule("WARNING_FLAGS", Translate::MapType::BitToString)
                 .set(MaskBits(0xffffffffffffffff))
                 .set(DefaultMessage("OK"))
                ));

       addStringFieldWithExtractorAndLookup(
            "information_flags",
            "Information flags.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(DifVifKey("07FFA800")),
            Translate::Lookup()
            .add(Translate::Rule("INFORMATION_FLAGS", Translate::MapType::BitToString)
                 .set(MaskBits(0xffffffffffffffff))
                 .set(DefaultMessage(""))
                ));

       addStringFieldWithExtractorAndLookup(
            "alarm_flags",
            "alarm flags.",
            DEFAULT_PRINT_PROPERTIES | PrintProperty::INJECT_INTO_STATUS,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(DifVifKey("07FFA900")),
            Translate::Lookup()
            .add(Translate::Rule("ALARM_FLAGS", Translate::MapType::BitToString)
                 .set(MaskBits(0xfffffffffffffff))
                 .set(DefaultMessage("OK"))
                ));

       addStringFieldWithExtractorAndLookup(
            "unknown_vif_FFAD",
            "Unknown byte.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(DifVifKey("01FFAD00")),
            Translate::Lookup()
            .add(Translate::Rule("UNKNOWN", Translate::MapType::BitToString)
                 .set(MaskBits(0xff))
                 .set(DefaultMessage("OK"))
                ));

        addStringFieldWithExtractor(
            "firmware_version",
            "Firmware version.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::FirmwareVersion)
            .add(VIFCombinableRaw(0))
            );

        addStringFieldWithExtractor(
            "product_no",
            "The meter device product number.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build().
            set(DifVifKey("0DFFAA00")));

        addNumericFieldWithExtractor(
            "power_fail",
            "Power fail counter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("04FF9800"))
            );

        addNumericFieldWithExtractor(
            "active_consumption",
            "Instantaneous total active imported power.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "active_consumption_l1",
            "Instantaneous active imported power for L1 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .add(VIFCombinableRaw(0x7f01))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "active_consumption_l2",
            "Instantaneous active imported power for L2 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .add(VIFCombinableRaw(0x7f02))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "active_consumption_l3",
            "Instantaneous active imported power for L3 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .add(VIFCombinableRaw(0x7f03))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "reactive_consumption",
            "Instantaneous total reactive imported power.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .set(SubUnitNr(2))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "reactive_consumption_l1",
            "Instantaneous reactive imported power for L1 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .set(SubUnitNr(2))
            .add(VIFCombinableRaw(0x7f01))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "reactive_consumption_l2",
            "Instantaneous reactive imported power for L2 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .set(SubUnitNr(2))
            .add(VIFCombinableRaw(0x7f02))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "reactive_consumption_l3",
            "Instantaneous reactive imported power for L3 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .set(SubUnitNr(2))
            .add(VIFCombinableRaw(0x7f03))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "apparent_consumption",
            "Instantaneous total apparent imported power.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .set(SubUnitNr(4))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "apparent_consumption_l1",
            "Instantaneous apparent imported power for L1 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .set(SubUnitNr(4))
            .add(VIFCombinableRaw(0x7f01))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "apparent_consumption_l2",
            "Instantaneous apparent imported power for L2 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .set(SubUnitNr(4))
            .add(VIFCombinableRaw(0x7f02))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "apparent_consumption_l3",
            "Instantaneous apparent imported power for L3 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .set(SubUnitNr(4))
            .add(VIFCombinableRaw(0x7f03))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "voltage_l1_n",
            "Instantaneous voltage between L1 and neutral.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Voltage)
            .add(VIFCombinableRaw(0x7f01))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "voltage_l2_n",
            "Instantaneous voltage between L2 and neutral.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Voltage)
            .add(VIFCombinableRaw(0x7f02))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "voltage_l3_n",
            "Instantaneous voltage between L3 and neutral.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Voltage)
            .add(VIFCombinableRaw(0x7f03))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "voltage_l1_l2",
            "Instantaneous voltage between L1 and L2.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Voltage)
            .add(VIFCombinableRaw(0x7f05))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "voltage_l2_l3",
            "Instantaneous voltage between L2 and L3.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Voltage)
            .add(VIFCombinableRaw(0x7f06))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "voltage_l3_l1",
            "Instantaneous voltage between L3 and L1.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Voltage)
            .add(VIFCombinableRaw(0x7f07))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "current_l1",
            "Instantaneous current in the L1 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Amperage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Amperage)
            .add(VIFCombinableRaw(0x7f01))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "current_l2",
            "Instantaneous current in the L2 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Amperage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Amperage)
            .add(VIFCombinableRaw(0x7f02))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "current_l3",
            "Instantaneous current in the L3 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Amperage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Amperage)
            .add(VIFCombinableRaw(0x7f03))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "frequency",
            "Frequency of AC",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Frequency,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("0AFFD900")),
            Unit::Hertz,
            0.01
            );

        addNumericFieldWithExtractor(
            "power",
            "Power factor.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("02FFE000")),
            Unit::FACTOR,
            0.001
            );

        addNumericFieldWithExtractor(
            "power_l1",
            "Power factor for phase L1.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("02FFE0FF8100")),
            Unit::FACTOR,
            0.001
            );

        addNumericFieldWithExtractor(
            "power_l2",
            "Power factor for phase L2.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("02FFE0FF8200")),
            Unit::FACTOR,
            0.001
            );

        addNumericFieldWithExtractor(
            "power_l3",
            "Power factor.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("02FFE0FF8300")),
            Unit::FACTOR,
            0.001
            );

        addNumericFieldWithExtractor(
            "power_phase_angle",
            "Total power phase angle.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Angle,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("02FFD200")),
            Unit::DEGREE,
            0.1
            );

        addNumericFieldWithExtractor(
            "phase_angle_power_l1",
            "Power phase angle for phase L1.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Angle,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("02FFD2FF8100")),
            Unit::DEGREE,
            0.1
            );

        addNumericFieldWithExtractor(
            "phase_angle_power_l2",
            "Power phase angle for phase L2.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Angle,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("02FFD2FF8200")),
            Unit::DEGREE,
            0.1
            );

        addNumericFieldWithExtractor(
            "phase_angle_power_l3",
            "Power phase angle for phase L3.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Angle,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("02FFD2FF8300")),
            Unit::DEGREE,
            0.1
            );

        addNumericFieldWithExtractor(
            "total_reactive_energy_consumption",
            "Total cumulative reactive kvarh imported energy.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Reactive_Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(2))
            .add(VIFCombinableRaw(0)),
            Unit::KVARH
            );

        addNumericFieldWithExtractor(
            "total_reactive_energy_consumption_tariff_{tariff_counter}",
            "Total cumulative reactive kvarh imported energy per tariff.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(2))
            .set(TariffNr(1),TariffNr(4))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "total_reactive_energy_production",
            "Total cumulative reactive kvarh exported energy.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Reactive_Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(3))
            .add(VIFCombinableRaw(0)),
            Unit::KVARH
            );

        addNumericFieldWithExtractor(
            "total_reactive_energy_production_tariff_{tariff_counter}",
            "Total cumulative reactive kvarh exported energy per tariff.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(3))
            .set(TariffNr(1),TariffNr(4))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "current_quadrant",
            "The quadrant in which the current is measured.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("01FF9700")),
            Unit::NUMBER
            );

        addNumericFieldWithExtractor(
            "current_quadrant_l1",
            "The quadrant in which the current is measured for phase L1.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("01FF97FF8100")),
            Unit::NUMBER
            );

        addNumericFieldWithExtractor(
            "current_quadrant_l2",
            "The quadrant in which the current is measured for phase L2.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("01FF97FF8200")),
            Unit::NUMBER
            );

        addNumericFieldWithExtractor(
            "current_quadrant_l3",
            "The quadrant in which the current is measured for phase L3.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("01FF97FF8300")),
            Unit::NUMBER
            );

        addStringFieldWithExtractorAndLookup(
            "digital_output_{subunit_counter}",
            "The state for output register 1-2.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DigitalOutput)
            .set(StorageNr(0))
            .set(TariffNr(0))
            .set(SubUnitNr(1),SubUnitNr(2))
            .add(VIFCombinableRaw(0)),
            Translate::Lookup()
            .add(Translate::Rule("OUTPUT", Translate::MapType::BitToString)
                 .set(MaskBits(0xff))
                ));

        addStringFieldWithExtractorAndLookup(
            "digital_input_{subunit_counter-2counter}",
            "The state for input register 1-2.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DigitalInput)
            .set(SubUnitNr(3),SubUnitNr(4))
            .add(VIFCombinableRaw(0)),
            Translate::Lookup()
            .add(Translate::Rule("INPUT", Translate::MapType::BitToString)
                 .set(MaskBits(0xff))
                ));

        addStringFieldWithExtractorAndLookup(
            "digital_historic_input_{subunit_counter-2counter}",
            "The state for input register 3-4.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DigitalInput)
            .set(StorageNr(1))
            .set(SubUnitNr(3),SubUnitNr(4))
            .add(VIFCombinableRaw(0)),
            Translate::Lookup()
            .add(Translate::Rule("INPUT", Translate::MapType::BitToString)
                 .set(MaskBits(0xff))
                ));

        addNumericFieldWithExtractor(
            "digital_input_{subunit_counter-2counter}",
            "Number of times input 1-2 counted a 1.",
             DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(SubUnitNr(3),SubUnitNr(4))
            .set(VIFRange::CumulationCounter)
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "resettable_energy_consumption",
            "Resettable cumulative active imported energy.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .add(VIFCombinableRaw(0x7f72))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "resettable_energy_production",
            "Resettable cumulative active exported energy.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(1))
            .add(VIFCombinableRaw(0x7f72))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "resettable_reactive_energy_consumption",
            "Resettable cumulative reactive imported energy.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(2))
            .add(VIFCombinableRaw(0x7f72))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "resettable_reactive_energy_production",
            "Resettable cumulative reactive exported energy.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(3))
            .add(VIFCombinableRaw(0x7f72))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "reset_energy_consumption",
            "Number of times the resettable energy imported value has been reset.",
             DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRaw(0x7f71))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "reset_energy_production",
            "Number of times the resettable active energy exported value has been reset.",
             DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRaw(0x7f71))
            .set(SubUnitNr(1))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "reset_reactive_energy_consumption",
            "Number of times the resettable reactive energy imported value has been reset.",
             DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRaw(0x7f71))
            .set(SubUnitNr(2))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "reset_reactive_energy_production",
            "Number of times the resettable reactive energy exported value has been reset.",
             DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRaw(0x7f71))
            .set(SubUnitNr(3))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "energy_co2",
            "Energy in co2.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Mass,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("0EFFF9C400")),
            Unit::KG,
            0.01
            );

        addNumericFieldWithExtractor(
            "co2_conversion",
            "CO2 conversion factor (kg * 10-3 /kWh).",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("04FFA400")),
            Unit::FACTOR
            );

        addNumericFieldWithExtractor(
            "energy_currency",
            "Energy in currency.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("0EFFF9C900"))
            );

        addNumericFieldWithExtractor(
            "currency_conversion",
            "Currency conversion factor (curr * 10-3 /kWh).",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("04FFA500")),
            Unit::FACTOR
            );

        addNumericFieldWithExtractor(
            "total_apparent_energy_consumption",
            "Total cumulative apparent kvah imported energy.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Apparent_Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(4))
            .add(VIFCombinableRaw(0)),
            Unit::KVAH
            );

        addNumericFieldWithExtractor(
            "total_apparent_energy_production",
            "Total cumulative apparent kvah exported energy.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Apparent_Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(5))
            .add(VIFCombinableRaw(0)),
            Unit::KVAH
            );

        addNumericFieldWithExtractor(
            "total_energy_consumption_l1",
            "Total imported active energy for L1 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .add(VIFCombinableRaw(0x7f01))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "total_energy_consumption_l2",
            "Total imported active energy for L2 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .add(VIFCombinableRaw(0x7f02))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "total_energy_consumption_l3",
            "Total imported active energy for L3 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .add(VIFCombinableRaw(0x7f03))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "total_reactive_energy_consumption_l1",
            "Total imported reactive energy for L1 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(2))
            .add(VIFCombinableRaw(0x7f01))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "total_reactive_energy_consumption_l2",
            "Total imported reactive energy for L2 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(2))
            .add(VIFCombinableRaw(0x7f02))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "total_reactive_energy_consumption_l3",
            "Total imported reactive energy for L3 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(2))
            .add(VIFCombinableRaw(0x7f03))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "total_apparent_energy_consumption_l1",
            "Total imported apparent energy for L1 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(4))
            .add(VIFCombinableRaw(0x7f01))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "total_apparent_energy_consumption_l2",
            "Total imported apparent energy for L2 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(4))
            .add(VIFCombinableRaw(0x7f02))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "total_apparent_energy_consumption_l3",
            "Total imported apparent energy for L3 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(4))
            .add(VIFCombinableRaw(0x7f03))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "total_energy_production_l1",
            "Total exported active energy for L1 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(1))
            .add(VIFCombinableRaw(0x7f01))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "total_energy_production_l2",
            "Total exported active energy for L2 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(1))
            .add(VIFCombinableRaw(0x7f02))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "total_energy_production_l3",
            "Total exported active energy for L3 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(1))
            .add(VIFCombinableRaw(0x7f03))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "total_reactive_energy_production_l1",
            "Total exported reactive energy for L1 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(3))
            .add(VIFCombinableRaw(0x7f01))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "total_reactive_energy_production_l2",
            "Total exported reactive energy for L2 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(3))
            .add(VIFCombinableRaw(0x7f02))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "total_reactive_energy_production_l3",
            "Total exported reactive energy for L3 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(3))
            .add(VIFCombinableRaw(0x7f03))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "total_apparent_energy_production_l1",
            "Total exported apparent energy for L1 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(5))
            .add(VIFCombinableRaw(0x7f01))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "total_apparent_energy_production_l2",
            "Total exported apparent energy for L2 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(5))
            .add(VIFCombinableRaw(0x7f02))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "total_apparent_energy_production_l3",
            "Total exported apparent energy for L3 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(5))
            .add(VIFCombinableRaw(0x7f03))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "total_net_energy",
            "Active net energy total.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(6))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "total_net_energy_l1",
            "Active net energy total for phase L1.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(6))
            .add(VIFCombinableRaw(0x7f01))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "total_net_energy_l2",
            "Active net energy total for phase L2.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(6))
            .add(VIFCombinableRaw(0x7f02))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "total_net_energy_l3",
            "Active net energy total for phase L3.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(6))
            .add(VIFCombinableRaw(0x7f03))
            .add(VIFCombinableRaw(0))
            );

        addNumericFieldWithExtractor(
            "total_net_reactive_energy",
            "Active net energy total.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(7))
            .add(VIFCombinableRaw(0)),
            Unit::KVARH
            );

        addNumericFieldWithExtractor(
            "total_net_reactive_energy_l1",
            "Active net reactive energy total for phase L1.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(7))
            .add(VIFCombinableRaw(0x7f01))
            .add(VIFCombinableRaw(0)),
            Unit::KVARH
            );

        addNumericFieldWithExtractor(
            "total_net_reactive_energy_l2",
            "Active net reactive energy total for phase L2.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(7))
            .add(VIFCombinableRaw(0x7f02))
            .add(VIFCombinableRaw(0)),
            Unit::KVARH
            );

        addNumericFieldWithExtractor(
            "total_net_reactive_energy_l3",
            "Active net reactive energy total for phase L3.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(7))
            .add(VIFCombinableRaw(0x7f03))
            .add(VIFCombinableRaw(0)),
            Unit::KVARH
            );

        addNumericFieldWithExtractor(
            "total_net_apparent_energy",
            "Active net energy total.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(8))
            .add(VIFCombinableRaw(0)),
            Unit::KVAH
            );

        addNumericFieldWithExtractor(
            "total_net_apparent_energy_l1",
            "Active net apparent energy total for phase L1.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(8))
            .add(VIFCombinableRaw(0x7f01))
            .add(VIFCombinableRaw(0)),
            Unit::KVAH
            );

        addNumericFieldWithExtractor(
            "total_net_apparent_energy_l2",
            "Active net apparent energy total for phase L2.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(8))
            .add(VIFCombinableRaw(0x7f02))
            .add(VIFCombinableRaw(0)),
            Unit::KVAH
            );

        addNumericFieldWithExtractor(
            "total_net_apparent_energy_l3",
            "Active net apparent energy total for phase L3.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(8))
            .add(VIFCombinableRaw(0x7f03))
            .add(VIFCombinableRaw(0)),
            Unit::KVAH
            );

    }
}

/* / Test: ABBmeter abbb23 33221100 NOKEY
  / telegram=|844442040011223320027A3E000020_0E840017495200000004FFA0150000000004FFA1150000000004FFA2150000000004FFA3150000000007FFA600000000000000000007FFA700000000000000000007FFA800000000000000000007FFA90000000000000000000DFD8E0007302E38322E31420DFFAA000B3030312D313131203332421F|
  / {"media":"electricity","meter":"abbb23","name":"ABBmeter","id":"33221100","total_energy_consumption_kwh":5249.17,"firmware_version": "B1.28.0","product_no": "B23 111-100","timestamp":"1111-11-11T11:11:11Z"}
 / |ABBmeter;33221100;5249.17;1111-11-11 11:11.11
*/
