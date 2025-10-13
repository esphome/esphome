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
        // This is the sharky 775 heat meter driver, should this merge with the sharky 774 driver?
        di.setName("sharky");
        di.setDefaultFields("name,id,total_energy_consumption_kwh,total_energy_consumption_tariff1_kwh,total_volume_m3,"
                            "total_volume_tariff2_m3,volume_flow_m3h,power_kw,flow_temperature_c,"
                            "return_temperature_c,temperature_difference_c,timestamp");
        di.setMeterType(MeterType::HeatMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_HYD, 0x04, 0x20);
        di.addDetection(MANUFACTURER_DME, 0x04, 0x40);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addOptionalLibraryFields("operating_time_h");

        addStringFieldWithExtractorAndLookup(
            "status",
            "Status of meter.",
            DEFAULT_PRINT_PROPERTIES  | PrintProperty::STATUS,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ErrorFlags),
            Translate::Lookup()
            .add(Translate::Rule("ERROR_FLAGS", Translate::MapType::BitToString)
                 .set(MaskBits(0x0000))
                 .set(DefaultMessage("OK"))
                ));

        addNumericFieldWithExtractor(
            "total_energy_consumption",
            "The total heat energy consumption recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            );

        addNumericFieldWithExtractor(
            "total_energy_consumption_tariff1",
            "The total heat energy consumption recorded by this meter on tariff 1.",
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
            "The total heating media volume recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            );

        addNumericFieldWithExtractor(
            "total_volume_tariff2",
            "The total heating media volume recorded by this meter on tariff 2.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(TariffNr(2))
            );

        addNumericFieldWithExtractor(
            "volume_flow",
            "The current heat media volume flow.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Flow,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::VolumeFlow)
            );

        addNumericFieldWithExtractor(
            "power",
            "The current power consumption.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::PowerW)
            );

        addNumericFieldWithExtractor(
            "flow_temperature",
            "The current forward heat media temperature.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::FlowTemperature)
            );

        addNumericFieldWithExtractor(
            "return_temperature",
            "The current return heat media temperature.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ReturnTemperature)
            );

        addNumericFieldWithExtractor(
            "temperature_difference",
            "The current return heat media temperature.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::TemperatureDifference)
            );

        addNumericFieldWithExtractor(
            "target_energy_consumption",
            "The total heat energy consumption recorded by this meter at the end of the previous billing period.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(StorageNr(5))
            );

        addNumericFieldWithExtractor(
            "target_volume",
            "The total heating media volume recorded by this meter at the end of the previous billing period.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(5))
            );

        addNumericFieldWithExtractor(
            "target",
            "The last billing period end date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::PointInTime,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(5)),
            Unit::DateLT);
    }
}

// Test: Heat sharky 68926025 NOKEY
// telegram=|534424232004256092687A370045752235854DEEEA5939FAD81C25FEEF5A23C38FB9168493C563F08DB10BAF87F660FBA91296BA2397E8F4220B86D3A192FB51E0BFCF24DCE72118E0C75A9E89F43BDFE370824B|
// {"media":"heat","meter":"sharky","name":"Heat","id":"68926025","total_energy_consumption_kwh":2651,"total_energy_consumption_tariff1_kwh":0,"total_volume_m3":150.347,"total_volume_tariff2_m3":0.018,"volume_flow_m3h":0,"power_kw":0,"flow_temperature_c":42.3,"return_temperature_c":28.1,"status":"OK","temperature_difference_c":14.1,"timestamp":"1111-11-11T11:11:11Z"}
// |Heat;68926025;2651;0;150.347;0.018;0;0;42.3;28.1;14.1;1111-11-11 11:11.11

// Test: Heat sharky 68926025 NOKEY
// telegram=|5e44a5115376916140047a0B0050052f2f0c0e829311008c100e000000000c14014938000c2B751400000B3B2902000a5a52070a5e95060a6256000a279015cc020e92831100cc021478113800c2026cdf2c2f2f2f2f2f2f2f2f2f2f2f2f2f|
// {"flow_temperature_c": 42.3,"id": "68926025","media": "heat","meter": "sharky","name": "Heat","power_kw": 0,"return_temperature_c": 28.1,"status": "OK","temperature_difference_c": 14.1,"timestamp": "1111-11-11T11:11:11Z","total_energy_consumption_kwh": 2651,"total_energy_consumption_tariff1_kwh": 0,"total_volume_m3": 150.347,"total_volume_tariff2_m3": 0.018,"volume_flow_m3h": 0}
// |Heat;68926025;2651;0;150.347;0.018;0;0;42.3;28.1;14.1;1111-11-11 11:11.11

// Test: Heato sharky 69696969 NOKEY
// telegram=|5e44a5116969696940047aBe0050052f2f0c06975100008c1006000000000c13849345000c2B000000000B3B0000000a5a06020a5e08020a6202f00B26110201cc020623500000cc021329554400c2026cdf2c2f2f2f2f2f2f2f2f2f2f2f2f|
// {"flow_temperature_c": 20.6,"id": "69696969","media": "heat","meter": "sharky","name": "Heato","operating_time_h": 10211,"power_kw": 0,"return_temperature_c": 20.8,"target_date": "2022-12-31","target_energy_consumption_kwh": 5023,"target_volume_m3": 445.529,"temperature_difference_c": -0.2,"timestamp": "1111-11-11T11:11:11Z","total_energy_consumption_kwh": 5197,"total_energy_consumption_tariff1_kwh": 0,"total_volume_m3": 459.384, "volume_flow_m3h": 0}
// |Heato;69696969;5197;0;459.384;null;0;0;20.6;20.8;-0.2;1111-11-11 11:11.11
