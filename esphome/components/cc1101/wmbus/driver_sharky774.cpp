/*
 Copyright (C) 2021 Vincent Privat (gpl-3.0-or-later)
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
        di.setName("sharky774");
        di.setDefaultFields("name,id,"
                            "total_energy_consumption_kwh,"
                            "energy_at_set_date_kwh,"
                            "set_date,"
                            "timestamp");
        di.setMeterType(MeterType::HeatMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_DME, 0x04,  0x41);
        di.addDetection(MANUFACTURER_DME, 0x0d,  0x41);
        di.addDetection(MANUFACTURER_DME, 0x0c,  0x41);
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
            "total_cooling_consumption",
            "The total cooling energy consumption recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(TariffNr(1))
            );

        addNumericFieldWithExtractor(
            "total_volume",
            "The total volume recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyVolumeVIF)
            );

        addNumericFieldWithExtractor(
            "total_cooling_volume",
            "The total cooling volume recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyVolumeVIF)
            .set(TariffNr(2))
            );

        addNumericFieldWithExtractor(
            "volume_flow",
            "The current flow.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Flow,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::VolumeFlow)
            );

        addNumericFieldWithExtractor(
            "power",
            "The power.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            );

        addNumericFieldWithExtractor(
            "flow_temperature",
            "The flow temperature.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::FlowTemperature)
            );

        addNumericFieldWithExtractor(
            "return_temperature",
            "The return temperature.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ReturnTemperature)
            );

        addNumericFieldWithExtractor(
            "operating_time",
            "How long the meter has been collecting data.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Time,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::OperatingTime)
            );

        addNumericFieldWithExtractor(
            "operating_time_in_error",
            "How long the meter has been in an error state and not collected data.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Time,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::OperatingTime)
            .add(VIFCombinable::RecordErrorCodeMeterToController)
            );

        addNumericFieldWithExtractor(
            "energy_at_set_date",
            "The total energy consumption recorded by this meter at the set date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "cooling_at_set_date",
            "The total cooling energy consumption recorded by this meter at the set date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(StorageNr(1))
            .set(TariffNr(1))
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
            .set(StorageNr(1)),
            Unit::DateLT
            );
    }
}

// Test: Heato sharky774 58496405 NOKEY
// telegram=3E44A5110564495841047A700030052F2F_0C06846800000C13195364000B3B0400000C2B110100000A5A17050A5E76020AA61800004C0647630000426CBF25
// {"media":"heat","meter":"sharky774","name":"Heato","id":"58496405","total_energy_consumption_kwh":6884,"total_volume_m3":645.319,"volume_flow_m3h":0.004,"power_kw":0.111,"flow_temperature_c":51.7,"return_temperature_c":27.6,"operating_time_in_error_h":0,"energy_at_set_date_kwh":6347,"set_date":"2021-05-31","timestamp":"1111-11-11T11:11:11Z"}
// |Heato;58496405;6884;6347;2021-05-31;1111-11-11 11:11.11

// Test: diehl_meter sharky774 52173898 NOKEY
// telegram=|3E44A51198381752410C7AA80030052F2F_0C06105104000C13093835020B3B9401000C2B342600000A5A12060A5E91040AA61800004C0641460400426CFF21|
// {"media":"heat volume at inlet","meter":"sharky774","name":"diehl_meter","id":"52173898","total_energy_consumption_kwh":45110,"total_volume_m3":2353.809,"volume_flow_m3h":0.194,"power_kw":2.634,"flow_temperature_c":61.2,"return_temperature_c":49.1,"operating_time_in_error_h":0,"energy_at_set_date_kwh":44641,"set_date":"2023-01-31","timestamp":"1111-11-11T11:11:11Z"}
// |diehl_meter;52173898;45110;44641;2023-01-31;1111-11-11 11:11.11

// This test telegram has more historical data!
// Test: Heatoo sharky774 72615127 NOKEY
// telegram=|5E44A5112751617241047A8B0050052F2F0C0E000000000C13010000000B3B0000000C2B000000000A5A26020A5E18020B260321000AA6180000C2026CBE2BCC020E00000000CC021301000000DB023B000000DC022B000000002F2F2F2F2F|
// {"media":"heat","meter":"sharky774","name":"Heatoo","id":"72615127","total_energy_consumption_kwh":0,"total_volume_m3":0.001,"volume_flow_m3h":0,"power_kw":0,"flow_temperature_c":22.6,"return_temperature_c":21.8,"operating_time_h":2103,"operating_time_in_error_h":0,"timestamp":"1111-11-11T11:11:11Z"}
// |Heatoo;72615127;0;null;null;1111-11-11 11:11.11

// This telegram contains a negative power value encoded in bcd.
// Test: Heatooo sharky774 61243590 NOKEY
// telegram=3E44A5119035246141047A1A0030052F2F_0C06026301000C13688609040B3B0802000C2B220000F00A5A71020A5E72020AA61800004C0636370100426CBF25
// {"media":"heat","meter":"sharky774","name":"Heatooo","id":"61243590","total_energy_consumption_kwh":16302,"total_volume_m3":4098.668,"volume_flow_m3h":0.208,"power_kw":-0.022,"flow_temperature_c":27.1,"return_temperature_c":27.2,"operating_time_in_error_h":0,"energy_at_set_date_kwh":13736,"set_date":"2021-05-31","timestamp":"1111-11-11T11:11:11Z"}
// |Heatooo;61243590;16302;13736;2021-05-31;1111-11-11 11:11.11

// This telegram contains cooling data as well.
// Test: Coolo sharky774 71942539 NOKEY
// telegram=5E44A51139259471410D7A720050052F2F_0C06742400008C1006000000000C13823522008C2013494400000B3B0000000C2B000000000A5A22030A5E91020AA61800004C0619130000CC100600000000426CDF252F2F2F2F2F2F2F2F2F2F2F
// {"cooling_at_set_date_kwh": 0,"energy_at_set_date_kwh": 1319,"flow_temperature_c": 32.2,"id": "71942539","media": "heat/cooling load","meter": "sharky774","name": "Coolo","operating_time_in_error_h": 0,"power_kw": 0,"return_temperature_c": 29.1,"set_date": "2022-05-31","timestamp": "1111-11-11T11:11:11Z","total_cooling_consumption_kwh": 0,"total_cooling_volume_m3": 4.449,"total_energy_consumption_kwh": 2474,"total_volume_m3": 223.582,"volume_flow_m3h": 0}
// |Coolo;71942539;2474;1319;2022-05-31;1111-11-11 11:11.11
