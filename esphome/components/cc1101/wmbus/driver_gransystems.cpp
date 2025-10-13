/*
 Copyright (C) 2018-2022 Fredrik Öhrström (gpl-3.0-or-later)

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

/*
Implemented January 2021 Xael South:
Implements GSS, CC101 and CC301 energy meter.
This T1 WM-Bus meter broadcasts:
- accumulated energy consumption
- accumulated energy consumption till yesterday
- current date
- actually measured voltage
- actually measured current
- actually measured frequency
- meter status and errors

The single-phase and three-phase send apparently the same datagram:
three-phase meter sends voltage and current values for every phase
L1 .. L3.

Meter version. Implementation tested against meters:
- CC101 one-phase with firmware version 0x01.
- CC301 three-phase with firmware version 0x01.

Encryption: None.
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
        di.setName("gransystems");
        di.setDefaultFields("name,id,total_energy_consumption_kwh,timestamp");
        di.setMeterType(MeterType::ElectricityMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_GSS, 0x02, 0x01);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addStringFieldWithExtractorAndLookup(
            "status",
            "Status of meter.",
            DEFAULT_PRINT_PROPERTIES | PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ErrorFlags),
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::MapType::BitToString,
                        AlwaysTrigger,
                        AutoMask,
                        "OK",
                        {
                            { 0x0001, "METER_HARDWARE_ERROR" },
                            { 0x0002, "RTC_ERROR" },
                            { 0x0100, "DSP_COMMUNICATION_ERROR" },
                            { 0x0200, "DSP_HARDWARE_ERROR" },
                            { 0x4000, "RAM_ERROR" },
                            { 0x8000, "ROM_ERROR" }
                        }
                    },
                    {
                        "ERROR_FLAGS_SINGLE_PHASE",
                        Translate::MapType::BitToString,
                        TriggerBits(0x01020000),
                        AutoMask,
                        "OK",
                        {
                            { 0x0008, "DEVICE_NOT_CONFIGURED" },
                            { 0x0010, "INTERNAL_ERROR" },
                            { 0x0020, "BATTERY_LOW" },
                            { 0x0040, "MAGNETIC_FRAUD_PRESENT" },
                            { 0x0080, "MAGNETIC_FRAUD_PAST" },
                        }
                    },
                    {
                        "ERROR_FLAGS_THREE_PHASE",
                        Translate::MapType::BitToString,
                        TriggerBits(0x01010000),
                        AutoMask,
                        "OK",
                        {
                            { 0x0008, "CALIBRATION_EEPROM_ERROR" },
                            { 0x0010, "NETWORK_INTERFERENCE" },
                            { 0x0800, "CALIBRATION_EEPROM_ERROR" },
                            { 0x1000, "EEPROM1_ERROR" }
                        }
                    },
                },
            });

        addStringFieldWithExtractorAndLookup(
            "info",
            "Is it a three phase or single phase meter.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ErrorFlags),
            {
                {
                    {
                        "INFO_FLAGS",
                        Translate::MapType::IndexToString,
                        AlwaysTrigger,
                        AutoMask,
                        "",
                        {
                            { 0x01020000, "SINGLE_PHASE_METER" },
                            { 0x01010000, "THREE_PHASE_METER" }
                        }
                    }
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
            "total_energy_consumption_tariff_{tariff_counter}",
            "The total energy consumption recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(TariffNr(1), TariffNr(4))
            );

        addNumericFieldWithExtractor(
            "target",
            "Last day?",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::PointInTime,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            .set(StorageNr(2))
            );

        addNumericFieldWithExtractor(
            "target_energy_consumption",
            "Last day energy consumption?",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(StorageNr(2))
            );

        addNumericFieldWithExtractor(
            "target_energy_consumption_tariff_{tariff_counter}",
            "Last day energy consumption for tariff?",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(TariffNr(1), TariffNr(4))
            .set(StorageNr(2))
            );

        addNumericFieldWithExtractor(
            "device",
            "Device date time when telegram was sent.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::PointInTime,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            );

        addNumericFieldWithExtractor(
            "voltage_at_phase_1",
            "Voltage for single phase meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Voltage)
            .set(SubUnitNr(0))
            );

        addNumericFieldWithExtractor(
            "voltage_at_phase_{subunit_counter}",
            "Voltage at phase L#.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Voltage)
            .set(SubUnitNr(1), SubUnitNr(3))
            );

        addNumericFieldWithExtractor(
            "current_at_phase_1",
            "Amperage for single phase meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Amperage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Amperage)
            .set(SubUnitNr(0))
            );

        addNumericFieldWithExtractor(
            "current_at_phase_{subunit_counter}",
            "Amperage at phase L#.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Amperage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Amperage)
            .set(SubUnitNr(1), SubUnitNr(3))
            );

        addNumericFieldWithExtractor(
            "raw_frequency",
            "Raw input to frequency.",
            DEFAULT_PRINT_PROPERTIES | PrintProperty::HIDE,
            Quantity::Frequency,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("02FB2D"))
            );

        addNumericFieldWithCalculator(
            "frequency",
            "Frequency of AC.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Frequency,
            "raw_frequency_hz / 100 counter");
    }
}

// Test: Gran101 gransystems 18046178 NOKEY
// Comment: Gran-System-S electricity meter 101
// telegram=|7844731e78610418010278046d0f13bc21040394030000841003690300008420032b00000084300300000000848010030000000084016d0000bc2184010394030000841103690300008421032b00000084310300000000848110030000000004fd482e09000004fd5b0000000002fb2d861304fd1700000201|
// {"media":"electricity","meter":"gransystems","name":"Gran101","id":"18046178","status":"OK","info":"SINGLE_PHASE_METER","total_energy_consumption_kwh":0.916,"total_energy_consumption_tariff_4_kwh":0,"total_energy_consumption_tariff_2_kwh":0.043,"total_energy_consumption_tariff_3_kwh":0,"total_energy_consumption_tariff_1_kwh":0.873,"target_datetime":"2021-01-28 00:00","target_energy_consumption_kwh":0.916,"target_energy_consumption_tariff_2_kwh":0.043,"target_energy_consumption_tariff_4_kwh":0,"target_energy_consumption_tariff_3_kwh":0,"target_energy_consumption_tariff_1_kwh":0.873,"device_datetime":"2021-01-28 19:15","voltage_at_phase_1_v":235,"current_at_phase_1_a":0,"frequency_hz":49.98,"timestamp":"1111-11-11T11:11:11Z"}
// |Gran101;18046178;0.916;1111-11-11 11:11.11

// Test: Gran301 gransystems 20100117 NOKEY
// Comment: Gran-System-S electricity meter 301
// telegram=|9e44731e17011020010278046d0813bc21040300000000841003000000008420030000000084300300000000848010030000000084016d0000bc218401030000000084110300000000842103000000008431030000000084811003000000008440fd4825090000848040fd480000000084c040fd48000000008440fd5b00000000848040fd5b0000000084c040fd5b0000000002fb2d881304fd1702000101|
// {"media":"electricity","meter":"gransystems","name":"Gran301","id":"20100117","status":"RTC_ERROR","info":"THREE_PHASE_METER","total_energy_consumption_kwh":0,"total_energy_consumption_tariff_2_kwh":0,"total_energy_consumption_tariff_3_kwh":0,"total_energy_consumption_tariff_4_kwh":0,"total_energy_consumption_tariff_1_kwh":0,"target_datetime":"2021-01-28 00:00","target_energy_consumption_kwh":0,"target_energy_consumption_tariff_1_kwh":0,"target_energy_consumption_tariff_2_kwh":0,"target_energy_consumption_tariff_4_kwh":0,"target_energy_consumption_tariff_3_kwh":0,"device_datetime":"2021-01-28 19:08","voltage_at_phase_2_v":0,"voltage_at_phase_3_v":0,"voltage_at_phase_1_v":234.1,"current_at_phase_1_a":0,"current_at_phase_2_a":0,"current_at_phase_3_a":0,"frequency_hz":50,"timestamp":"1111-11-11T11:11:11Z"}
// |Gran301;20100117;0;1111-11-11 11:11.11
