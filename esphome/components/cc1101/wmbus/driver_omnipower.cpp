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

#include"meters_common_implementation.h"

/*
Implemented October 2020 Janus Bo Andersen:
Implements Kamstrup OmniPower, energy meter.
This C1 WM-Bus meter broadcasts:
- Accumulated energy consumption (A+, kWh)
- Accumulated energy production (A-, kWh)
- Current power consumption (P+, kW)
- Current power production (P-, kW)

According to Kamstrup doc. 58101496_C1_GB_05.2018
(Wireless M-Bus Module for OMNIPOWER), the single-phase,
three-phase and CT meters send the same datagram.

Meter version. Implementation tested against meter:
Kamstrup one-phase with firmware version 0x30.

Meter uses AES-128 in CTR mode, which is the only mode supported by
the extended link layer (wm-bus), see EN 13757-4:2019.

*/
namespace
{
    struct Driver : public virtual MeterCommonImplementation
    {
        Driver(MeterInfo &mi, DriverInfo &di);
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("omnipower");
        di.setDefaultFields("name,id,total_energy_consumption_kwh,total_energy_production_kwh,"
                            "current_power_consumption_kw,current_power_production_kw,timestamp");
        di.setMeterType(MeterType::ElectricityMeter);
        di.addLinkMode(LinkMode::C1);

        di.addDetection(MANUFACTURER_KAM, 0x02,  0x30);

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
            "current_power_consumption",
            "The current power consumption.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            );

        addNumericFieldWithExtractor(
            "current_power_production",
            "The current power production.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .add(VIFCombinable::BackwardFlow)
            );
    }
}
// Test: myomnipower omnipower 32666857 NOKEY
// Comment:
// telegram=|2D442D2C5768663230028D20E4E2C81C20878C78_04041A03000004843C00000000042B0300000004AB3C00000000|
// {"media":"electricity","meter":"omnipower","name":"myomnipower","id":"32666857","total_energy_consumption_kwh":7.94,"total_energy_production_kwh":0,"current_power_consumption_kw":0.003,"current_power_production_kw":0,"timestamp":"1111-11-11T11:11:11Z"}
// |myomnipower;32666857;7.94;0;0.003;0;1111-11-11 11:11.11
// telegram=|27442D2C5768663230028D20E900C91C2011BA79138CCCFB_1A030000000000000300000000000000|
// {"media":"electricity","meter":"omnipower","name":"myomnipower","id":"32666857","total_energy_consumption_kwh":7.94,"total_energy_production_kwh":0,"current_power_consumption_kw":0.003,"current_power_production_kw":0,"timestamp":"1111-11-11T11:11:11Z"}
// |myomnipower;32666857;7.94;0;0.003;0;1111-11-11 11:11.11
