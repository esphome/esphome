/*
 Copyright (C) 2021 Fredrik Öhrström (gpl-3.0-or-later)

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
        di.setName("qsmoke");
        di.setDefaultFields("name,id,status,last_alarm_date,alarm_counter,timestamp");
        di.setMeterType(MeterType::SmokeDetector);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_QDS, 0x1a,  0x21);
        di.addDetection(MANUFACTURER_QDS, 0x1a,  0x23);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addStringFieldWithExtractorAndLookup(
            "status",
            "Meter error flags. IMPORTANT! Smoke alarm is probably NOT reported here! You MUST check last alarm date and counter!",
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
                        AlwaysTrigger, MaskBits(0xffff),
                        "OK",
                        {
                        }
                    },
                },
            }));

        addStringFieldWithExtractor(
            "last_alarm_date",
            "Date when the smoke alarm last triggered.",
             DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(StorageNr(6))
            .set(VIFRange::Date)
            );

        addNumericFieldWithExtractor(
            "alarm",
            "Number of times the smoke alarm has triggered.",
             DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Unsigned,
            FieldMatcher::build()
            .set(DifVifKey("81037C034C4123"))
            );

        addStringFieldWithExtractor(
            "message_datetime",
            "Device date time.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            );

        addStringFieldWithExtractor(
            "test_button_last_date",
            "Date when test button was last pressed.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(StorageNr(4))
            .set(VIFRange::Date)
            );

        addNumericFieldWithExtractor(
            "test_button",
            "Number of times the test button has been pressed.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("81027C03495523"))
            );

        addNumericFieldWithExtractor(
            "transmission",
            "Transmission counter?",
             DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Unsigned,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AccessNumber)
            );

        addStringFieldWithExtractor(
            "at_error_date",
            "Date when the device entered an error state.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::AtError)
            .set(VIFRange::Date)
            );

        addNumericFieldWithExtractor(
            "some_sort_of_duration",
            "What does this mean?",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Time,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("02FDAC7E"))
            );

    }
}

// Test: QSmokeo qsmoke 45797086 NOKEY
// telegram=|3E44934486707945211A7801FD08F081027C034955230082026CFFFF81037C034C41230082036CFFFF03FD17000000326CFFFF046D0F0ABC2B02FDAC7E1100|
// {"media":"smoke detector","meter":"qsmoke","name":"QSmokeo","id":"45797086","status":"OK","last_alarm_date":"2127-15-31","alarm_counter":0,"message_datetime":"2021-11-28 10:15","test_button_last_date":"2127-15-31","test_button_counter":0,"transmission_counter":240,"at_error_date":"2127-15-31","some_sort_of_duration_h":0.004722,"timestamp":"1111-11-11T11:11:11Z"}
// |QSmokeo;45797086;OK;2127-15-31;0;1111-11-11 11:11.11

// Test: QSmokep qsmoke 48128850 NOKEY
// telegram=|3744934450881248231A7A5C00002081027C034955230082026CFFFF81037C034C41230082036CFFFF02FD170000326CFFFF046D2514BC2B|
// {"media":"smoke detector","meter":"qsmoke","name":"QSmokep","id":"48128850","status":"OK","last_alarm_date":"2127-15-31","alarm_counter":0,"message_datetime":"2021-11-28 20:37","test_button_last_date":"2127-15-31","test_button_counter":0,"at_error_date":"2127-15-31","timestamp":"1111-11-11T11:11:11Z"}
// |QSmokep;48128850;OK;2127-15-31;0;1111-11-11 11:11.11
