/*
 Copyright (C) 2019-2022 Fredrik Öhrström (gpl-3.0-or-later)

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
        di.setName("rfmamb");
        di.setDefaultFields("name,id,current_temperature_c,current_relative_humidity_rh,timestamp");
        di.setMeterType(MeterType::TempHygroMeter);
        di.addLinkMode(LinkMode::T1);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
        di.addDetection(MANUFACTURER_BMT, 0x1b,  0x10);
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addStringField(
            "status",
            "Meter status from tpl status field.",
            DEFAULT_PRINT_PROPERTIES  |
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

        addNumericFieldWithExtractor(
            "average_temperature_24h",
            "The average temperature over the last 24 hours.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ExternalTemperature)
            .set(StorageNr(2))
            );

        addNumericFieldWithExtractor(
            "maximum_temperature_1h",
            "The maximum temperature over the last hour.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Maximum)
            .set(VIFRange::ExternalTemperature)
            );

        addNumericFieldWithExtractor(
            "maximum_temperature_24h",
            "The maximum temperature over the last 24 hours.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Maximum)
            .set(VIFRange::ExternalTemperature)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "minimum_temperature_1h",
            "The minimum temperature over the last hour.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Minimum)
            .set(VIFRange::ExternalTemperature)
            );

        addNumericFieldWithExtractor(
            "minimum_temperature_24h",
            "The minimum temperature over the last 24 hours.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Minimum)
            .set(VIFRange::ExternalTemperature)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "current_relative_humidity",
            "The current relative humidity.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::RelativeHumidity,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::RelativeHumidity)
            );

        addNumericFieldWithExtractor(
            "average_relative_humidity_1h",
            "The average relative humidity over the last hour.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::RelativeHumidity,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::RelativeHumidity)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "average_relative_humidity_24h",
            "The average relative humidity over the last 24 hours.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::RelativeHumidity,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::RelativeHumidity)
            .set(StorageNr(2))
            );

        addNumericFieldWithExtractor(
            "maximum_relative_humidity_1h",
            "The maximum relative humidity over the last hour.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::RelativeHumidity,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Maximum)
            .set(VIFRange::RelativeHumidity)
            );

        addNumericFieldWithExtractor(
            "maximum_relative_humidity_24h",
            "The maximum relative humidity over the last 24 hours.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::RelativeHumidity,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Maximum)
            .set(VIFRange::RelativeHumidity)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "minimum_relative_humidity_1h",
            "The minimum relative humidity over the last hour.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::RelativeHumidity,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Minimum)
            .set(VIFRange::RelativeHumidity)
            );

        addNumericFieldWithExtractor(
            "minimum_relative_humidity_24h",
            "The minimum relative humidity over the last 24 hours.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::RelativeHumidity,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Minimum)
            .set(VIFRange::RelativeHumidity)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "device",
            "The meters date time.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::PointInTime,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            );
    }
}

// Test: Rummet rfmamb 11772288 NOKEY
// telegram=|5744b40988227711101b7ab20800000265a00842658f088201659f08226589081265a0086265510852652b0902fb1aba0142fb1ab0018201fb1abd0122fb1aa90112fb1aba0162fb1aa60152fb1af501066d3b3bb36b2a00|
// {"media":"room sensor","meter":"rfmamb","name":"Rummet","id":"11772288","status":"PERMANENT_ERROR","current_temperature_c":22.08,"average_temperature_1h_c":21.91,"average_temperature_24h_c":22.07,"maximum_temperature_1h_c":22.08,"minimum_temperature_1h_c":21.85,"maximum_temperature_24h_c":23.47,"minimum_temperature_24h_c":21.29,"current_relative_humidity_rh":44.2,"average_relative_humidity_1h_rh":43.2,"average_relative_humidity_24h_rh":44.5,"minimum_relative_humidity_1h_rh":42.5,"maximum_relative_humidity_1h_rh":44.2,"maximum_relative_humidity_24h_rh":50.1,"minimum_relative_humidity_24h_rh":42.2,"device_datetime":"2019-10-11 19:59","timestamp":"1111-11-11T11:11:11Z"}
// |Rummet;11772288;22.08;44.2;1111-11-11 11:11.11
