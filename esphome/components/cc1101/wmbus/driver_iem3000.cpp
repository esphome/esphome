/*
 Copyright (C) 2024 Fredrik Öhrström (gpl-3.0-or-later)

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
        di.setName("iem3000");
        di.setDefaultFields("name,id,total_energy_consumption_kwh,timestamp");
        di.setMeterType(MeterType::ElectricityMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_SEC,  0x02,  0x13);
        di.addDetection(MANUFACTURER_SEC,  0x02,  0x15);
        di.addDetection(MANUFACTURER_SEC,  0x02,  0x18);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addOptionalLibraryFields("firmware_version,manufacturer,meter_datetime,model_version");

        addStringField(
            "status",
            "Status and error flags.",
            PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS);

        addStringFieldWithExtractorAndLookup(
            "error_flags",
            "Error flags.",
            PrintProperty::INJECT_INTO_STATUS,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ErrorFlags),
            Translate::Lookup()
            .add(Translate::Rule("ERROR_FLAGS", Translate::MapType::IndexToString)
                 .set(MaskBits(0xffffff))
                 .add(Translate::Map(0x000000, "CODE_101_EEPROM_ERROR", TestBit::Set))
                 .add(Translate::Map(0x000010, "CODE_102_NO_CALIBRATION_TABLE", TestBit::Set))
                 .add(Translate::Map(0x000020, "CODE_201_MISMATCH_BETWEEN_FREQUENCY_SETTINGS_AND_FREQUENCY_MEASUREMENTS", TestBit::Set))
                 .add(Translate::Map(0x000030, "CODE_202_PHASE_SEQUENCE_REVERSED", TestBit::Set))
                 .add(Translate::Map(0x000040, "CODE_203_PHASE_SEQUENCE_REVERSED", TestBit::Set))
                 .add(Translate::Map(0x000050, "CODE_204_TOTAL_ACTIVE_ENERGY_NEGATIVE_DUE_TO_INCORRECT_V_OR_A_CONNECTIONS", TestBit::Set))
                 .add(Translate::Map(0x000060, "CODE_205_DATE_TIME_RESET_DUE_TO_POWER_FAILUER", TestBit::Set))
                 .add(Translate::Map(0x000070, "CODE_206_PULSE_MISSING_DUE_TO_OVERSPEED_OF_ENERGY_PULSE_OUTPUT", TestBit::Set))
                 .add(Translate::Map(0x000080, "CODE_207_ABNORMAL_INTERNAL_CLOCK_FUNCTION", TestBit::Set))
                 .add(Translate::Map(0x000090, "INTERNAL_DATA_BUS_COMUNICATION_ERROR", TestBit::Set))
                ));

        addNumericFieldWithExtractor(
            "total_energy_consumption",
            "Total cumulative active imported energy.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            );

        addNumericFieldWithExtractor(
            "partial_energy_consumption",
            "Partial cumulative active imported energy.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .add(VIFCombinableRaw(0x7f0d))
            );

        addNumericFieldWithExtractor(
            "partial_reactive_energy_consumption",
            "Partial cumulative reactive imported energy.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(1))
            .add(VIFCombinableRaw(0x7f0d))
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
            .add(VIFCombinableRaw(0x7f09))
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
            );

        addNumericFieldWithExtractor(
            "active_tariff",
            "Active tariff.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("03FF10")),
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
            Unit::NUMBER
            );

        addNumericFieldWithExtractor(
            "vt_numerator",
            "Voltage transformer ratio (numerator).",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("04FFA115")),
            Unit::NUMBER
            );

        addNumericFieldWithExtractor(
            "ct_denominator",
            "Current transformer ratio (denominator).",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("04FFA215")),
            Unit::NUMBER
            );

        addNumericFieldWithExtractor(
            "vt_denominator",
            "Voltage transformer ratio (denominator).",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("04FFA315")),
            Unit::NUMBER
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
            .set(SubUnitNr(1))
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
            .set(SubUnitNr(2))
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
            );

        addNumericFieldWithExtractor(
            "voltage_average_ln",
            "Average voltage line to neutral.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Voltage)
            .add(VIFCombinableRaw(0x7f04))
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
            );

        addNumericFieldWithExtractor(
            "voltage_average_ll",
            "Average voltage line to line.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Voltage)
            .add(VIFCombinableRaw(0x7f08))
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
            );

        addNumericFieldWithExtractor(
            "current_average",
            "Average current in all phases.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Amperage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Amperage)
            .add(VIFCombinableRaw(0x7f00))
            );

        addNumericFieldWithExtractor(
            "frequency",
            "Frequency of AC",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Frequency,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("05FF0B")),
            Unit::Hertz);

        addNumericFieldWithExtractor(
            "power",
            "Power factor.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("05FF0A")),
            Unit::FACTOR);

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
            .set(SubUnitNr(1)),
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
            .set(SubUnitNr(1))
            .add(VIFCombinableRaw(0x7f09)),
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

        addNumericFieldWithExtractor(
            "input_metering_cumulation",
            "Input metering accumulation.",
             DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::CumulationCounter)
            );

        addNumericFieldWithExtractor(
            "pulse_duration",
            "Energy pulse duration.",
             DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(DifVifKey("03FF2C"))
            );

        addNumericFieldWithExtractor(
            "pulse_weight",
            "Energy pulse weight.",
             DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(DifVifKey("05FF2E"))
            );

        addNumericFieldWithExtractor(
            "pulse_constant",
            "Energy pulse constant.",
             DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(DifVifKey("05FF2F"))
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
            .set(SubUnitNr(4)),
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
            .set(SubUnitNr(5)),
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
            .set(SubUnitNr(7)),
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
            .add(VIFCombinableRaw(0x7f01)),
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
            .add(VIFCombinableRaw(0x7f02)),
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
            .add(VIFCombinableRaw(0x7f03)),
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
            .set(SubUnitNr(8)),
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
            .add(VIFCombinableRaw(0x7f01)),
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
            .add(VIFCombinableRaw(0x7f02)),
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
            .add(VIFCombinableRaw(0x7f03)),
            Unit::KVAH
            );

        addNumericFieldWithExtractor(
            "last_partial_energy_reset",
            "Date and time of last partial energy reset,",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::PointInTime,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            .add(VIFCombinableRaw(0x7f0c))
            );

        addNumericFieldWithExtractor(
            "last_input_metering_reset",
            "Date and time of last input metering reset.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::PointInTime,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            .add(VIFCombinableRaw(0x7f0e))
            );

        addStringFieldWithExtractorAndLookup(
            "digital_input",
            "Digital input status.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DigitalInput),
            Translate::Lookup()
            .add(Translate::Rule("INPUT", Translate::MapType::BitToString)
                 .set(MaskBits(0xffffff))
                ));

        addStringFieldWithExtractorAndLookup(
            "digital_input_status",
            "Digital input status.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(DifVifKey("02FF32")),
            Translate::Lookup()
            .add(Translate::Rule("INPUT_STATUS", Translate::MapType::BitToString)
                 .set(MaskBits(0xff))
                ));

        addStringFieldWithExtractorAndLookup(
            "digital_output",
            "Digital output status.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DigitalOutput),
            Translate::Lookup()
            .add(Translate::Rule("OUTPUT", Translate::MapType::BitToString)
                 .set(MaskBits(0xffffff))
                ));

        addStringFieldWithExtractorAndLookup(
            "digital_output_association",
            "Digital output association.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(DifVifKey("03FF2D")),
            Translate::Lookup()
            .add(Translate::Rule("OUTPUT_ASSOCIATION", Translate::MapType::BitToString)
                 .set(MaskBits(0xffffff))
                ));

        addStringFieldWithExtractorAndLookup(
            "digital_input_association",
            "Digital input association.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(DifVifKey("03FF30")),
            Translate::Lookup()
            .add(Translate::Rule("INPUT_ASSOCIATION", Translate::MapType::BitToString)
                 .set(MaskBits(0xffffff))
                ));

        addStringFieldWithExtractorAndLookup(
            "digital_output_association",
            "Digital output association.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(DifVifKey( "02FF36")),
            Translate::Lookup()
            .add(Translate::Rule("OUTPUT_ASSOCIATION", Translate::MapType::BitToString)
                 .set(MaskBits(0xffff))
                ));

        addStringFieldWithExtractorAndLookup(
            "overload_alarm_setup",
            "Overload alarm setup.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(DifVifKey("02FF34")),
            Translate::Lookup()
            .add(Translate::Rule("OVERLOAD_ALARM", Translate::MapType::BitToString)
                 .set(MaskBits(0xff))
                ));

        addNumericFieldWithExtractor(
            "pickup_setpoint",
            "Pickup setpoint.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("05FF35")));

        addStringFieldWithExtractorAndLookup(
            "activated_status",
            "Activated status.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(DifVifKey("02FF37")),
            Translate::Lookup()
            .add(Translate::Rule("ACTIVATED_STATUS", Translate::MapType::BitToString)
                 .set(MaskBits(0xff))
                ));

        addStringFieldWithExtractorAndLookup(
            "unack_status",
            "Unacknowledged status.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(DifVifKey("02FF38")),
            Translate::Lookup()
            .add(Translate::Rule("UNACK_STATUS", Translate::MapType::BitToString)
                 .set(MaskBits(0xff))
                ));

        addNumericFieldWithExtractor(
            "last_alarm",
            "Date and time of last alarm.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::PointInTime,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            .add(VIFCombinableRaw(0x7f39))
            );

        addNumericFieldWithExtractor(
            "last_alarm",
            "Last alarm value.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("05FF3A")),
            Unit::NUMBER);

        addNumericFieldWithExtractor(
            "operating_time",
            "Operating time. Unit is unknown, please fix!",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Time,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("06FF20")),
            Unit::Year);

        addNumericFieldWithExtractor(
            "phases",
            "Number of phases.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("03FF21")));

        addNumericFieldWithExtractor(
            "wires",
            "Number of wires.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("03FF22")),
            Unit::NUMBER);

        addNumericFieldWithExtractor(
            "vts",
            "Number of voltaget transformers VT:s.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("03FF25")));

        addNumericFieldWithExtractor(
            "vt_primary",
            "Primary voltage transformer VT.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("05FF26")),
            Unit::NUMBER);

        addNumericFieldWithExtractor(
            "vt_secondary",
            "Secondary voltage transformer VT.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("03FF27")),
            Unit::NUMBER);

        addNumericFieldWithExtractor(
            "cts",
            "Number of current transformers CT:s.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("03FF28")));

        addNumericFieldWithExtractor(
            "ct_primary",
            "Primary current transformer CT.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("03FF29")),
            Unit::NUMBER);

        addNumericFieldWithExtractor(
            "ct_secondary",
            "Secondary current transformer CT.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("03FF2A")),
            Unit::NUMBER);

        addNumericFieldWithExtractor(
            "vt_connection_type",
            "Voltage transformer connection type.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("03FF2B")),
            Unit::NUMBER);

        addStringFieldWithExtractorAndLookup(
            "power_system_configuration",
            "Power system configuration.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(DifVifKey("03FF23")),
            Translate::Lookup()
            .add(Translate::Rule("POWER_SYS_CONFIG", Translate::MapType::BitToString)
                 .set(MaskBits(0xffffff))
                ));

        addNumericFieldWithExtractor(
            "nominal_frequency",
            "Nominal frequency.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Frequency,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("03FF24")),
            Unit::Hertz);

    }
}
