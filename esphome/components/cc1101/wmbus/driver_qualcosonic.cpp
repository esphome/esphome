/*
 Copyright (C) 2022-2024 Fredrik Öhrström (gpl-3.0-or-later)

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
        di.setName("qualcosonic");
        di.setDefaultFields("name,id,status,total_heat_energy_kwh,total_cooling_energy_kwh,"
                            "power_kw,target_datetime,target_heat_energy_kwh,target_cooling_energy_kwh,timestamp");
        di.setMeterType(MeterType::HeatCoolingMeter);
        di.addLinkMode(LinkMode::C1);
        di.addDetection(MANUFACTURER_AXI, 0x0d,  0x0b);
        di.addDetection(MANUFACTURER_AXI, 0x0d,  0x0c);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) :  MeterCommonImplementation(mi, di)
    {
        addOptionalLibraryFields("fabrication_no,operating_time_h,on_time_h,meter_datetime,meter_datetime_at_error");
        addOptionalLibraryFields("total_m3,flow_temperature_c,return_temperature_c,flow_return_temperature_difference_c,volume_flow_m3h");

        addStringFieldWithExtractorAndLookup(
            "status",
            "Meter status. Includes both meter error field and tpl status field.",
            DEFAULT_PRINT_PROPERTIES   |
            PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS,
            FieldMatcher::build()
            .set(VIFRange::ErrorFlags),
            Translate::Lookup(
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::MapType::BitToString,
                        AlwaysTrigger, MaskBits(0xffffffff),
                        "OK",
                        {
                            /* What do these bits mean? There are a lot of them...
                            */
                        }
                    },
                },
            }));

        addNumericFieldWithExtractor(
            "total_heat_energy",
            "The total heating energy consumption recorded by this meter.",
             DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .add(VIFCombinable::ForwardFlow)
            );

        addNumericFieldWithExtractor(
            "total_cooling_energy",
            "The total cooling energy consumption recorded by this meter.",
             DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .add(VIFCombinable::BackwardFlow)
            );

        addNumericFieldWithExtractor(
            "power",
            "The current power consumption.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            );

        addStringFieldWithExtractor(
            "target_datetime",
            "Date and time when the previous billing period ended.",
             DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            .set(StorageNr(16))
            );

        addNumericFieldWithExtractor(
            "target_heat_energy",
            "The heating energy consumption recorded at the end of the previous billing period.",
             DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .add(VIFCombinable::ForwardFlow)
            .set(StorageNr(16))
            );

        addNumericFieldWithExtractor(
            "target_cooling_energy",
            "The cooling energy consumption recorded at the end of the previous billing period.",
             DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .add(VIFCombinable::BackwardFlow)
            .set(StorageNr(16))
            );
    }
}

// Test: qualco qualcosonic 03016408 NOKEY
// telegram=|76440907086401030B0d7a78000000046d030fB726346d0000010134fd170000000004200f13cf0104240f13cf0104863B0000000004863cdc0000000413B5150600042B86f1ffff043B030200000259c002025d2c05026194fd0c780864010384086d3B17Bf258408863B000000008408863c0B000000|
// {"media":"heat/cooling load","meter":"qualcosonic","name":"qualco","id":"03016408","fabrication_no":"03016408","operating_time_h":8430.013056,"on_time_h":8430.013056,"meter_datetime":"2021-06-23 15:03","meter_datetime_at_error":"2000-01-01 00:00","total_m3":398.773,"flow_temperature_c":7.04,"return_temperature_c":13.24,"flow_return_temperature_difference_c":-6.2,"volume_flow_m3h":0.515,"status":"OK","total_heat_energy_kwh":0,"total_cooling_energy_kwh":220,"power_kw":-3.706,"target_datetime":"2021-05-31 23:59","target_heat_energy_kwh":0,"target_cooling_energy_kwh":11,"timestamp":"1111-11-11T11:11:11Z"}
// |qualco;03016408;OK;0;220;-3.706;2021-05-31 23:59;0;11;1111-11-11 11:11.11

// Test: qualcoe4 qualcosonic 29481002 NOKEY
// telegram=|76440907021048290C0D7A89000000_046D2301E122346D0000010134FD17000000000420097468020424DE736802048E3BF1190000048E3C00000000041323860100042B6E050000043B2500000002598016025D000A0261800C0C780210482984086D3B17FF2184088E3BED19000084088E3C00000000|
// {"media":"heat/cooling load","meter":"qualcosonic","name":"qualcoe4","id":"29481002","fabrication_no":"29481002","operating_time_h":11222.177222,"on_time_h":11222.189167,"meter_datetime":"2023-02-01 01:35","meter_datetime_at_error":"2000-01-01 00:00","total_m3":99.875,"flow_temperature_c":57.6,"return_temperature_c":25.6,"flow_return_temperature_difference_c":32,"volume_flow_m3h":0.037,"status":"OK","total_heat_energy_kwh":1844.722222,"total_cooling_energy_kwh":0,"power_kw":1.39,"target_datetime":"2023-01-31 23:59","target_heat_energy_kwh":1843.611111,"target_cooling_energy_kwh":0,"timestamp":"1111-11-11T11:11:11Z"}
// |qualcoe4;29481002;OK;1844.722222;0;1.39;2023-01-31 23:59;1843.611111;0;1111-11-11 11:11.11

// Test: qualcoe4long qualcosonic 98499485 NOKEY
// telegram=|9A440907859449980C0D7A8C000000_046D3201E122346D0000010134FD17000000000420BB776802042490776802048E3BF2190000048E3C00000000041329860100042B6A010000043B0B0000000259F615025D1E0B0261D80A0C788594499884086D3B17FF21820859BC1382085D720B840824915D680284088E3BED19000084088E3C00000000840813028601008408BE5800000000027F00AD|
// {"media":"heat/cooling load","meter":"qualcosonic","name":"qualcoe4long","id":"98499485","fabrication_no":"98499485","operating_time_h":11222.44,"on_time_h":11222.451944,"meter_datetime":"2023-02-01 01:50","meter_datetime_at_error":"2000-01-01 00:00","total_m3":99.881,"flow_temperature_c":56.22,"return_temperature_c":28.46,"flow_return_temperature_difference_c":27.76,"volume_flow_m3h":0.011,"status":"OK","total_heat_energy_kwh":1845,"total_cooling_energy_kwh":0,"power_kw":0.362,"target_datetime":"2023-01-31 23:59","target_heat_energy_kwh":1843.611111,"target_cooling_energy_kwh":0,"timestamp":"1111-11-11T11:11:11Z"}
// |qualcoe4long;98499485;OK;1845;0;0.362;2023-01-31 23:59;1843.611111;0;1111-11-11 11:11.11
