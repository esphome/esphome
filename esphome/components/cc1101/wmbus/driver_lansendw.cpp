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
        di.setName("lansendw");
        di.setDefaultFields("name,id,status,timestamp");
        di.setMeterType(MeterType::DoorWindowDetector);
        di.addLinkMode(LinkMode::T1);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
        di.addDetection(MANUFACTURER_LAS,  0x1d,  0x07);
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addStringFieldWithExtractorAndLookup(
            "status",
            "The state (OPEN/CLOSED) for the door/window.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DigitalInput),
            Translate::Lookup()
            .add(Translate::Rule("INPUT_BITS", Translate::MapType::IndexToString)
                 .set(MaskBits(0xffff))
                 .add(Translate::Map(0x11 ,"CLOSED", TestBit::Set))
                 .add(Translate::Map(0x55 ,"OPEN", TestBit::Set))
                ));

        addStringFieldWithExtractorAndLookup(
            "error_flags",
            "Error flags.",
            PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ErrorFlags),
            Translate::Lookup()
            .add(Translate::Rule("ERROR_FLAGS", Translate::MapType::BitToString)
                 .set(MaskBits(0xffff))
                 .set(DefaultMessage("OK"))
                ));

        addNumericFieldWithExtractor(
            "a",
            "How many times the door/window has been opened or closed.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Dimensionless)
            );

        addNumericFieldWithExtractor(
            "b",
            "The current number of counted pulses from counter b.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Dimensionless)
            .set(SubUnitNr(1))
            );

    }
}

// Test: Dooro lansendw 00010205 NOKEY
// telegram=|2e44333005020100071d7ab54800002f2f02fd1b110002fd971d01000efd3a2200000000008e40fd3a000000000000|
// {"media":"reserved","meter":"lansendw","name":"Dooro","id":"00010205","status":"CLOSED","a_counter":22,"b_counter":0,"error_flags":"ERROR_FLAGS_1 PERMANENT_ERROR UNKNOWN_40","timestamp":"1111-11-11T11:11:11Z"}
// |Dooro;00010205;CLOSED;1111-11-11 11:11.11

// telegram=|2e44333005020100071d7ab66800002f2f02fd1b550002fd971d01000efd3a2300000000008e40fd3a000000000000|
// {"media":"reserved","meter":"lansendw","name":"Dooro","id":"00010205","status":"OPEN","a_counter":23,"b_counter":0,"error_flags":"ERROR_FLAGS_1 PERMANENT_ERROR UNKNOWN_60","timestamp":"1111-11-11T11:11:11Z"}
// |Dooro;00010205;OPEN;1111-11-11 11:11.11
