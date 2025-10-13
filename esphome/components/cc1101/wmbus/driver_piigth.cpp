/*
 Copyright (C) 2021-2022 Fredrik Öhrström (gpl-3.0-or-later)

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
        di.setName("piigth");
        di.setDefaultFields("name,id,status,temperature_c,relative_humidity_rh,timestamp");
        di.setMeterType(MeterType::TempHygroMeter);
        di.addLinkMode(LinkMode::MBUS);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
        di.addDetection(MANUFACTURER_PII,  0x1b,  0x01);
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addOptionalLibraryFields("fabrication_no,software_version");

        addStringField(
            "status",
            "Meter status from tpl status field.",
            PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS);

        addNumericFieldWithExtractor(
            "temperature",
            "The current temperature.",
            PrintProperty::REQUIRED,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ExternalTemperature)
            );

        addNumericFieldWithExtractor(
            "average_temperature_1h",
            "The average temperature over the last hour.",
            PrintProperty::REQUIRED,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ExternalTemperature)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "average_temperature_24h",
            "The average temperature over the last 24 hours.",
            PrintProperty::REQUIRED,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ExternalTemperature)
            .set(StorageNr(2))
            );

        addNumericFieldWithExtractor(
            "relative_humidity",
            "The current relative humidity.",
            PrintProperty::REQUIRED,
            Quantity::RelativeHumidity,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::RelativeHumidity)
            );

        addNumericFieldWithExtractor(
            "relative_humidity_1h",
            "The average relative humidity over the last hour.",
            PrintProperty::REQUIRED,
            Quantity::RelativeHumidity,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::RelativeHumidity)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "relative_humidity_24h",
            "The average relative humidity over the last 24 hours.",
            PrintProperty::REQUIRED,
            Quantity::RelativeHumidity,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::RelativeHumidity)
            .set(StorageNr(2))
            );
    }
}


// Test: Tempo piigth 10000284 NOKEY
// telegram=|68383868080072840200102941011B04000000_0265C0094265A509B20165000002FB1A900142FB1A6901B201FB1A00000C788402001002FD0F21000FC016|
// {"media":"room sensor","meter":"piigth","name":"Tempo","id":"10000284","fabrication_no":"10000284","software_version":"0021","status":"OK","temperature_c":24.96,"average_temperature_1h_c":24.69,"relative_humidity_rh":40,"relative_humidity_1h_rh":36.1,"timestamp":"1111-11-11T11:11:11Z"}
// |Tempo;10000284;OK;24.96;40;1111-11-11 11:11.11

// telegram=|68383868080072840200102941011B06000000_02653F0A4265000A820165CA0902FB1A4F0142FB1A53018201FB1A5E010C788402001002FD0F21000F1916|
// {"media":"room sensor","meter":"piigth","name":"Tempo","id":"10000284","fabrication_no":"10000284","software_version":"0021","status":"OK","temperature_c":26.23,"average_temperature_1h_c":25.6,"average_temperature_24h_c":25.06,"relative_humidity_rh":33.5,"relative_humidity_1h_rh":33.9,"relative_humidity_24h_rh":35,"timestamp":"1111-11-11T11:11:11Z"}
// |Tempo;10000284;OK;26.23;33.5;1111-11-11 11:11.11
