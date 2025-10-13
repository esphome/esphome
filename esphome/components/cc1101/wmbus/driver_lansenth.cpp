/*
 Copyright (C) 2019-2023 Fredrik Öhrström (gpl-3.0-or-later)

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
        di.setName("lansenth");
        di.setDefaultFields("name,id,current_temperature_c,current_relative_humidity_rh,timestamp");
        di.setMeterType(MeterType::TempHygroMeter);
        di.addDetection(MANUFACTURER_LAS,  0x1b,  0x07);
        di.addDetection(MANUFACTURER_LAS,  0x1b,  0x09);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addOptionalLibraryFields("on_time_h");
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
            "current_relative_humidity",
             "The current humidity.",
             DEFAULT_PRINT_PROPERTIES,
            Quantity::RelativeHumidity,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::RelativeHumidity)
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
            "average_relative_humidity_1h",
             "The average humidity over the last hour.",
             DEFAULT_PRINT_PROPERTIES,
            Quantity::RelativeHumidity,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::RelativeHumidity)
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
            "average_relative_humidity_24h",
             "The average humidity over the last 24 hours.",
             DEFAULT_PRINT_PROPERTIES,
            Quantity::RelativeHumidity,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::RelativeHumidity)
            .set(StorageNr(2))
            );

    }
}


// Test: Tempoo lansenth 00010203 NOKEY
// telegram=|2e44333003020100071b7a634820252f2f0265840842658308820165950802fb1aae0142fb1aae018201fb1aa9012f|
// {"media":"room sensor","meter":"lansenth","name":"Tempoo","id":"00010203","status":"PERMANENT_ERROR SABOTAGE_ENCLOSURE","current_temperature_c":21.8,"current_relative_humidity_rh":43,"average_temperature_1h_c":21.79,"average_relative_humidity_1h_rh":43,"average_temperature_24h_c":21.97,"average_relative_humidity_24h_rh":42.5,"timestamp":"1111-11-11T11:11:11Z"}
// |Tempoo;00010203;21.8;43;1111-11-11 11:11.11

// Test: T2 lansenth 00060041 NOKEY
// telegram=|2E44333041000600091B7AA70020252F2F_0265DBF94265FC04820165610901FB1B2C41FB1B238101FB1B290223BB00|+0
// {"media":"room sensor","meter":"lansenth","name":"T2","id":"00060041","status":"OK","current_temperature_c":-15.73,"current_relative_humidity_rh":44,"average_temperature_1h_c":12.76,"average_relative_humidity_1h_rh":35,"average_temperature_24h_c":24.01,"average_relative_humidity_24h_rh":41,"on_time_h":4488,"timestamp":"1111-11-11T11:11:11Z"}
// |T2;00060041;-15.73;44;1111-11-11 11:11.11
