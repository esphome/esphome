/*
 Copyright (C) 2017-2022 Fredrik Öhrström (gpl-3.0-or-later)
 Copyright (C) 2018 David Mallon (gpl-3.0-or-later)

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
        di.setName("iperl");
        di.setDefaultFields("name,id,total_m3,max_flow_m3h,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_SEN,  0x06,  0x68);
        di.addDetection(MANUFACTURER_SEN,  0x07,  0x68);
        di.addDetection(MANUFACTURER_SEN,  0x07,  0x7c);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addNumericFieldWithExtractor(
            "total",
            "The total water consumption recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            );

        addNumericFieldWithExtractor(
            "max_flow",
            "The maximum flow recorded during previous period.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Flow,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::VolumeFlow));
    }
}

// Test: MoreWater iperl 12345699 NOKEY
// Comment: Test iPerl T1 telegram, that after decryption, has 2f2f markers.
// telegram=|1E44AE4C9956341268077A36001000_2F2F0413181E0000023B00002F2F2F2F|
// {"media":"water","meter":"iperl","name":"MoreWater","id":"12345699","total_m3":7.704,"max_flow_m3h":0,"timestamp":"1111-11-11T11:11:11Z"}
// |MoreWater;12345699;7.704;0;1111-11-11 11:11.11

// Test: WaterWater iperl 33225544 NOKEY
// Comment: Test iPerl T1 telegram not encrypted, which has no 2f2f markers.
// telegram=|1844AE4C4455223368077A55000000_041389E20100023B0000|
// {"media":"water","meter":"iperl","name":"WaterWater","id":"33225544","total_m3":123.529,"max_flow_m3h":0,"timestamp":"1111-11-11T11:11:11Z"}
// |WaterWater;33225544;123.529;0;1111-11-11 11:11.11
