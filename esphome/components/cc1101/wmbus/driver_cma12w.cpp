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
        di.setName("cma12w");
        di.setDefaultFields("name,id,current_temperature_c,timestamp");
        di.setMeterType(MeterType::TempHygroMeter);
        di.addLinkMode(LinkMode::C1);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_ELV,  0x1b,  0x20);

        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });

    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addOptionalLibraryFields("software_version");

        addStringField(
            "status",
            "Meter status from tpl status field.",
            PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS);

        addNumericFieldWithExtractor(
            "current_temperature",
            "The current temperature.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ExternalTemperature)
            );

        addNumericFieldWithExtractor(
            "average_temperature_1h",
            "The average temperature over the last hour.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ExternalTemperature)
            .set(StorageNr(1))
            );

        addStringFieldWithExtractorAndLookup(
            "battery",
            "Battery status.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DigitalInput),
            Translate::Lookup()
            .add(Translate::Rule("BATTERY", Translate::MapType::BitToString)
                 .set(MaskBits(0xffff)))
            );
    }
}


// Test: Tempo cma12w 66666666 NOKEY
// telegram=|2744961566666666201B7AF9000020_2F2F02651E094265180902FD1B30030DFD0F05302E302E340F|
// {"media":"room sensor","meter":"cma12w","name":"Tempo","id":"66666666","software_version":"4.0.0","status":"OK","current_temperature_c":23.34,"average_temperature_1h_c":23.28,"battery":"BATTERY_330","timestamp":"1111-11-11T11:11:11Z"}
// |Tempo;66666666;23.34;1111-11-11 11:11.11
