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
        di.setName("lansenpu");
        di.setDefaultFields("name,id,status,a_counter,b_counter,timestamp");
        di.setMeterType(MeterType::PulseCounter);
        di.addDetection(MANUFACTURER_LAS,  0x00,  0x14);
        di.addDetection(MANUFACTURER_LAS,  0x00,  0x1b);
        di.addDetection(MANUFACTURER_LAS,  0x02,  0x0b);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        setMfctTPLStatusBits(
            Translate::Lookup()
            .add(Translate::Rule("TPL_STS", Translate::MapType::BitToString)
                 .set(MaskBits(0xe0))
                 .set(DefaultMessage("OK"))
                 .add(Translate::Map(0x40 ,"SABOTAGE_ENCLOSURE", TestBit::Set))));

        addStringField(
            "status",
            "Meter status from tpl status field.",
            DEFAULT_PRINT_PROPERTIES   |
            PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS);

        /* Doubles have a 52 bit significand 11 bit exp and 1 bit sign,
           so double is good for incremental pulses up to 2^52 counts
           which is approx 4.5*10^15
           The values sent by this meter are 12 digit bcd, ie at most 10^13-1 counts.
           so they fit inside a double. */
        addNumericFieldWithExtractor(
            "a",
             "The current number of counted pulses from counter a.",
             DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("0EFD3A"))
            );

        addNumericFieldWithExtractor(
            "b",
             "The current number of counted pulses from counter b.",
             DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("8E40FD3A"))
            );
    }
}

// Test: COUNTA lansenpu 00010206 NOKEY
// telegram=|234433300602010014007a8e0400002f2f0efd3a1147000000008e40fd3a341200000000|
// {"media":"other","meter":"lansenpu","name":"COUNTA","id":"00010206","status":"POWER_LOW","a_counter":4711,"b_counter":1234,"timestamp":"1111-11-11T11:11:11Z"}
// |COUNTA;00010206;POWER_LOW;4711;1234;1111-11-11 11:11.11


// Test: COUNTB lansenpu 00023750 NOKEY
// telegram=|1A443330503702000B027AD7000020|2F2F8E40FD3A700800000000|
// {"media":"electricity","meter":"lansenpu","name":"COUNTB","id":"00023750","status":"OK","b_counter":870,"timestamp":"1111-11-11T11:11:11Z"}
// |COUNTB;00023750;OK;null;870;1111-11-11 11:11.11

// telegram=|1A443330503702000B027AD74c0020|2F2F8E40FD3A700800000000|
// {"media":"electricity","meter":"lansenpu","name":"COUNTB","id":"00023750","status":"PERMANENT_ERROR POWER_LOW SABOTAGE_ENCLOSURE","b_counter":870,"timestamp":"1111-11-11T11:11:11Z"}
// |COUNTB;00023750;PERMANENT_ERROR POWER_LOW SABOTAGE_ENCLOSURE;null;870;1111-11-11 11:11.11
