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
        di.setName("ebzwmbe");
        di.setDefaultFields("name,id,total_energy_consumption_kwh,current_power_consumption_kw,current_power_consumption_phase1_kw,current_power_consumption_phase2_kw,current_power_consumption_phase3_kw,timestamp");
        di.setMeterType(MeterType::ElectricityMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_EBZ, 0x02, 0x01);
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
            "current_power_consumption_phase1",
            "Current power consumption at phase 1.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("04A9FF01"))
            );

        addNumericFieldWithExtractor(
            "current_power_consumption_phase2",
            "Current power consumption at phase 2.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("04A9FF02"))
            );

        addNumericFieldWithExtractor(
            "current_power_consumption_phase3",
            "Current power consumption at phase 3.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("04A9FF03"))
            );

        addStringFieldWithExtractor(
            "customer",
            "Customer name.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Customer)
            );

/*
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
*/

        addNumericFieldWithCalculator(
            "current_power_consumption",
            "Calculated sum of power consumption of all phases.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            "current_power_consumption_phase1_kw + current_power_consumption_phase2_kw + current_power_consumption_phase3_kw"
            );

    }
}

// Test: Elen1 ebzwmbe 22992299 NOKEY
// telegram=|5B445A149922992202378C20F6900F002C25BC9E0000BF48954821BC508D72992299225A140102F6003007102F2F040330F92A0004A9FF01FF24000004A9FF026A29000004A9FF03460600000DFD11063132333435362F2F2F2F2F2F|
// {"media":"electricity","meter":"ebzwmbe","name":"Elen1","id":"22992299","total_energy_consumption_kwh":2816.304,"current_power_consumption_phase1_kw":0.09471,"current_power_consumption_phase2_kw":0.10602,"current_power_consumption_phase3_kw":0.01606,"customer":"654321","current_power_consumption_kw":0.21679,"timestamp":"1111-11-11T11:11:11Z"}
// |Elen1;22992299;2816.304;0.21679;0.09471;0.10602;0.01606;1111-11-11 11:11.11

// Test: MyEl ebzwmbe 01135263 NOKEY
// Comment: This is eBZ DD3 meter with a wMB-E01 wmbus radio converter module.
// telegram=|4B445A146450000002378C20F3900F002C25F38B00006CBB07CEB215901172635213015A140102F3002007102F2F#04039D3E03000429E82201000DFD110E333632353331313031305A424531|
// {"media":"electricity","meter":"ebzwmbe","name":"MyEl","id":"01135263","total_energy_consumption_kwh":212.637,"customer":"1EBZ0101135263","current_power_consumption_kw":null,"timestamp":"1111-11-11T11:11:11Z"}
// |MyEl;01135263;212.637;null;null;null;null;1111-11-11 11:11.11
