/*
 Copyright (C) 2019-2022 Fredrik Öhrström (gpl-3.0-or-later)

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
        di.setName("amiplus");
        di.setDefaultFields("name,id,total_energy_consumption_kwh,current_power_consumption_kw,total_energy_production_kwh,current_power_production_kw,voltage_at_phase_1_v,voltage_at_phase_2_v,voltage_at_phase_3_v,total_energy_consumption_tariff_1_kwh,total_energy_consumption_tariff_2_kwh,total_energy_consumption_tariff_3_kwh,total_energy_production_tariff_1_kwh,total_energy_production_tariff_2_kwh,total_energy_production_tariff_3_kwh,timestamp");
        di.setMeterType(MeterType::ElectricityMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_APA,  0x02,  0x02);
        di.addDetection(MANUFACTURER_DEV,  0x37,  0x02);
        di.addDetection(MANUFACTURER_DEV,  0x02,  0x00);
        // Apator Otus 1/3 seems to use both, depending on a frame.
        // Frames with APA are successfully decoded by this driver
        // Frames with APT are not - and their content is unknown - perhaps it broadcasts two data formats?
        di.addDetection(MANUFACTURER_APA,  0x02,  0x01);
        di.addDetection(0x14ed,  0x02,  0x01);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
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
            "current_power_consumption",
            "Current power consumption.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::PowerW)
            );

        addNumericFieldWithExtractor(
            "total_energy_production",
            "The total energy production recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("0E833C"))
            );

        addNumericFieldWithExtractor(
            "current_power_production",
            "Current power production.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("0BAB3C"))
            );

        addNumericFieldWithExtractor(
            "voltage_at_phase_1",
            "Voltage at phase L1.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("0AFDC9FC01"))
            );

        addNumericFieldWithExtractor(
            "voltage_at_phase_2",
            "Voltage at phase L2.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("0AFDC9FC02"))
            );

        addNumericFieldWithExtractor(
            "voltage_at_phase_3",
            "Voltage at phase L3.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("0AFDC9FC03"))
            );

        addStringFieldWithExtractor(
            "device_date_time",
            "Device date time.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            );

        addNumericFieldWithExtractor(
            "total_energy_consumption_tariff_1",
            "The total energy consumption recorded by this meter on tariff 1.",
            DEFAULT_PRINT_PROPERTIES, // ,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(TariffNr(1))
            );

        addNumericFieldWithExtractor(
            "total_energy_consumption_tariff_2",
            "The total energy consumption recorded by this meter on tariff 2.",
            DEFAULT_PRINT_PROPERTIES, // ,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(TariffNr(2))
            );

        addNumericFieldWithExtractor(
            "total_energy_consumption_tariff_3",
            "The total energy consumption recorded by this meter on tariff 3.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(TariffNr(3))
            );

        addNumericFieldWithExtractor(
            "total_energy_production_tariff_1",
            "The total energy production recorded by this meter on tariff 1.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("8E10833C"))
            );

        addNumericFieldWithExtractor(
            "total_energy_production_tariff_2",
            "The total energy production recorded by this meter on tariff 2.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("8E20833C"))
            );

        addNumericFieldWithExtractor(
            "total_energy_production_tariff_3",
            "The total energy production recorded by this meter on tariff 3.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("8E30833C"))
            );

        addNumericFieldWithExtractor(
            "max_power_consumption",
            "The maximum demand indicator (maximum 15-min average power consumption recorded this month).",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Maximum)
            .set(VIFRange::AnyPowerVIF)
            );

    }
}

// Test: MyElectricity1 amiplus 10101010 NOKEY
// telegram=|4E4401061010101002027A00004005_2F2F0E035040691500000B2B300300066D00790C7423400C78371204860BABC8FC100000000E833C8074000000000BAB3C0000000AFDC9FC0136022F2F2F2F2F|
// {"media":"electricity","meter":"amiplus","name":"MyElectricity1","id":"10101010","total_energy_consumption_kwh":15694.05,"current_power_consumption_kw":0.33,"total_energy_production_kwh":7.48,"current_power_production_kw":0,"voltage_at_phase_1_v":236,"device_date_time":"2019-03-20 12:57:00","timestamp":"1111-11-11T11:11:11Z"}
// |MyElectricity1;10101010;15694.05;0.33;7.48;0;236;null;null;null;null;null;null;null;null;1111-11-11 11:11.11

// Test: MyElectricity2 amiplus 00254358 NOKEY
// Comment: amiplus/apator electricity meter with three phase voltages
// telegram=|5E44B6105843250000027A2A005005_2F2F0C7835221400066D404708AC2A400E032022650900000E833C0000000000001B2B9647000B2B5510000BAB3C0000000AFDC9FC0135020AFDC9FC0245020AFDC9FC0339020BABC8FC100000002F2F|
// {"media":"electricity","meter":"amiplus","name":"MyElectricity2","id":"00254358","total_energy_consumption_kwh":9652.22,"current_power_consumption_kw":1.055,"total_energy_production_kwh":0,"current_power_production_kw":0,"voltage_at_phase_1_v":235,"voltage_at_phase_2_v":245,"voltage_at_phase_3_v":239,"max_power_consumption_kw":4.796,"device_date_time":"2021-10-12 08:07:00","timestamp":"1111-11-11T11:11:11Z"}
// |MyElectricity2;00254358;9652.22;1.055;0;0;235;245;239;null;null;null;null;null;null;1111-11-11 11:11.11

// Test: MyElectricity3 amiplus 86064864 NOKEY
// Comment: amiplus/apator electricity meter with three phase voltages and 2 tariffs.
// telegram=|804401066448068602027A000070052F2F_066D1E5C11DA21400C78644806868E10036110012500008E20038106531800008E10833C9949000000008E20833C8606000000001B2B5228020B2B3217000BAB3C0000000AFDC9FC0131020AFDC9FC0225020AFDC9FC0331020BABC8FC100000002F2F2F2F2F2F2F2F2F2F2F2F2FDE47|
// {"media":"electricity","meter":"amiplus","name":"MyElectricity3","id":"86064864","current_power_consumption_kw":1.732,"current_power_production_kw":0,"voltage_at_phase_1_v":231,"voltage_at_phase_2_v":225,"voltage_at_phase_3_v":231,"device_date_time":"2022-01-26 17:28:30","total_energy_consumption_tariff_1_kwh":25011.061,"total_energy_consumption_tariff_2_kwh":18530.681,"total_energy_production_tariff_1_kwh":4.999,"total_energy_production_tariff_2_kwh":0.686,"max_power_consumption_kw":22.852,"timestamp":"1111-11-11T11:11:11Z"}
// |MyElectricity3;86064864;null;1.732;null;0;231;225;231;25011.061;18530.681;null;4.999;0.686;null;1111-11-11 11:11.11

// Test: MyElectricity4 amiplus 55090884 NOKEY
// Comment: amiplus/apator electricity meter with single phase voltage - Otus 1
// telegram=|7E4401068408095501027A7C1070052F2F_066DDE5E150D39800C78840809550AFDC9FC0139028E30833C0000000000008E20833C0000000000008E10833C4301000000000BABC8FC100000008E10035336420200008E20030000000000008E30030000000000000B2B9502000BAB3C0000002F2F2F2F2F2F2F2F2F2F2F2F2F|
// {"media":"electricity","meter":"amiplus","name":"MyElectricity4","id":"55090884","current_power_consumption_kw":0.295,"current_power_production_kw":0,"total_energy_consumption_tariff_1_kwh":2423.653,"total_energy_consumption_tariff_2_kwh":0,"total_energy_consumption_tariff_3_kwh":0,"total_energy_production_tariff_1_kwh":0.143,"total_energy_production_tariff_2_kwh":0,"total_energy_production_tariff_3_kwh":0,"voltage_at_phase_1_v":239,"device_date_time":"2024-09-13 21:30:30","timestamp":"1111-11-11T11:11:11Z"}
// |MyElectricity4;55090884;null;0.295;null;0;239;null;null;2423.653;0;0;0.143;0;0;1111-11-11 11:11.11
