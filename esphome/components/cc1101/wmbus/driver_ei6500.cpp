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
        di.setName("ei6500");
        di.setDefaultFields("name,id,status,last_alarm_date,alarm_counter,timestamp");
        di.setMeterType(MeterType::SmokeDetector);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_EIE, 0x1a, 0x0c);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        setMfctTPLStatusBits(
            Translate::Lookup()
            .add(Translate::Rule("TPL_STS", Translate::MapType::BitToString)
                 .set(MaskBits(0xe0))
                 .set(DefaultMessage("OK"))
                 .add(Translate::Map(0x04 ,"RTC_INVALID", TestBit::Set))));

        addStringFieldWithExtractorAndLookup(
            "status",
            "Meter error flags. IMPORTANT! Smoke alarm is NOT reported here! You MUST check last alarm date and counter!",
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
                            /* When used for m-bus?
                               bit  7-7 Not used, zero by default
                               bit  8 Application error, unknown field C
                               bit  9 Application error, unknown field CI
                               bit 10 Application error, unknown record
                               bit 11 Application error, access right
                               bit 12 Application error, record size
                               bit 13 Application error, record value
                               bit 14 Application error, bad password
                               bit 15 Not used, zero by defaul
                            */
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
            .set(SubUnitNr(1))
            .set(TariffNr(1))
            .set(VIFRange::Date)
            );

        addNumericFieldWithExtractor(
            "alarm",
            "Number of times the smoke alarm has triggered.",
             DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(SubUnitNr(1))
            .set(TariffNr(1))
            .set(VIFRange::CumulationCounter)
            );

        addStringFieldWithExtractor(
            "software_version",
            "Meter software version number.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::SoftwareVersion)
            );

        addStringFieldWithExtractor(
            "message_datetime",
            "Device date time.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            );

        addNumericFieldWithExtractor(
            "duration_removed",
            "Time the smoke alarm has been removed.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Time,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(SubUnitNr(1))
            .set(TariffNr(2))
            .set(VIFRange::DurationOfTariff)
            );

        addStringFieldWithExtractor(
            "last_remove_date",
            "Date when the smoke alarm was last removed.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(SubUnitNr(1))
            .set(TariffNr(2))
            .set(VIFRange::Date)
            );

        addNumericFieldWithExtractor(
            "removed",
            "Number of times the smoke alarm has been removed.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(SubUnitNr(1))
            .set(TariffNr(2))
            .set(VIFRange::CumulationCounter)
            );

        addStringFieldWithExtractor(
            "test_button_last_date",
            "Date when test button was last pressed.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(SubUnitNr(1))
            .set(TariffNr(3))
            .set(VIFRange::Date)
            );

        addNumericFieldWithExtractor(
            "test_button",
            "Number of times the test button has been pressed.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(SubUnitNr(1))
            .set(TariffNr(3))
            .set(VIFRange::CumulationCounter)
            );

        addStringFieldWithExtractor(
            "installation_date",
            "Date when the smoke alarm was installed.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(TariffNr(2))
            .set(VIFRange::Date)
            );

        addStringFieldWithExtractor(
            "last_sound_check_date",
            "Date when the smoke alarm last checked the piezo speaker.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(StorageNr(1))
            .set(VIFRange::Date)
            );

        addStringFieldWithExtractorAndLookup(
            "dust_level",
            "Dust level 0 (best) to 15 (worst).",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(DifVifKey("8440FF2C")),
            Translate::Lookup(
            {
                {
                    {
                        "DUST",
                        Translate::MapType::IndexToString,
                        AlwaysTrigger, MaskBits(0x1f),
                        "",
                        {
                        }
                    },
                },
            }));

        addStringFieldWithExtractorAndLookup(
            "battery_level",
            "Battery voltage level.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(DifVifKey("8440FF2C")),
            Translate::Lookup(
            {
                {
                    {
                        "BATTERY_VOLTAGE",
                        Translate::MapType::IndexToString,
                        AlwaysTrigger, MaskBits(0x0f00),
                        "",
                        {
                            { 0x0000, "2.25V" },
                            { 0x0100, "2.30V" },
                            { 0x0200, "2.35V" },
                            { 0x0300, "2.40V" },

                            { 0x0400, "2.45V" },
                            { 0x0500, "2.50V" },
                            { 0x0600, "2.55V" },
                            { 0x0700, "2.60V" },

                            { 0x0800, "2.65V" },
                            { 0x0900, "2.70V" },
                            { 0x0a00, "2.75V" },
                            { 0x0b00, "2.80V" },

                            { 0x0c00, "2.85V" },
                            { 0x0d00, "2.90V" },
                            { 0x0e00, "2.95V" },
                            { 0x0f00, "3.00V" }
                        }
                    },
                },
            }));

        addStringFieldWithExtractorAndLookup(
            "obstacle_distance",
            "The distance to a detected obstacle.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(DifVifKey("8440FF2C")),
            Translate::Lookup(
            {
                {
                    {
                        "OBSTACLE_DISTANCE",
                        Translate::MapType::IndexToString,
                        AlwaysTrigger, MaskBits(0x700000),
                        "",
                        {
                            { 0x000000, "SEODS_NOT_COMPLETED" },
                            { 0x100000, "" }, // No obstacle detected
                            { 0x200000, "45_TO_60_CM" },
                            { 0x300000, "38_TO_53_CM" },

                            { 0x400000, "33_TO_48_CM" },
                            { 0x500000, "28_TO_40_CM" },
                            { 0x600000, "20_TO_33_CM" },
                            { 0x700000, "0_TO_25_CM" },
                        }
                    },
                },
            }));


        addStringFieldWithExtractorAndLookup(
            "head_status",
            "Status of smoke detector sensors, merged into the status field.",
            PrintProperty::INJECT_INTO_STATUS | PrintProperty::HIDE,
            FieldMatcher::build()
            .set(DifVifKey("8440FF2C")),
            Translate::Lookup(
            {
                {
                    {
                        "HEAD_STATUS",
                        Translate::MapType::BitToString,
                        AlwaysTrigger, MaskBits(0xff8ff0e0),
                        "OK",
                        {
                           /* 0x00000000-0x000000f dust level*/
                            { 0x00000020, "SOUNDER_FAULT" },
                            { 0x00000040, "TAMPER_WHILE_REMOVED" },
                            { 0x00000080, "EOL_REACHED" },
                           /* 0x00000100-0x000f00 battery evel*/
                            { 0x00001000, "LOW_BATTERY_FAULT" },
                            { 0x00002000, "ALARM_SENSOR_FAULT" },
                            { 0x00004000, "OBSTACLE_DETECTOR_FAULT" },
                            { 0x00008000, "EOL_WITHIN_12_MONTH" },
                            { 0x00010000, "SEODS_NOT_YET_COMPLETED", TestBit::NotSet },
                            { 0x00020000, "ENV_CHANGED_SINCE_INSTALLATION" },
                            { 0x00040000, "COMM_TO_HEAD_FAULT" },
                            { 0x00080000, "INTERFERENCE_PREVENTING_OBSTACLE_DETECTION" },
                           /* 0x00100000-0x0700000 distance */
                           /* 0x00800000 reserved */
                            { 0x01000000, "OBSTACLE_DETECTED" },
                            { 0x02000000, "SMOKE_DETECTOR_FULLY_COVERED" }
                        }
                    },
                },
            }));

    }
}

// Test: Smokey ei6500 01097274 NOKEY
// telegram=|58442515747209010C1A7A8B0000000BFD0F070101046D2A06D82502FD17000082206CD825426CD0238440FF2C000F11008250FD61000082506C01018260FD6100008360FD3100000082606C01018270FD61000082706C0101|
// {"media":"smoke detector","meter":"ei6500","name":"Smokey","id":"01097274","status":"OK","last_alarm_date":"2000-01-01","alarm_counter":0,"software_version":"010107","message_datetime":"2022-05-24 06:42","duration_removed_h":0,"last_remove_date":"2000-01-01","removed_counter":0,"test_button_last_date":"2000-01-01","test_button_counter":0,"installation_date":"2022-05-24","last_sound_check_date":"2022-03-16","dust_level":"DUST_0","battery_level":"3.00V","obstacle_distance":"","timestamp":"1111-11-11T11:11:11Z"}
// |Smokey;01097274;OK;2000-01-01;0;1111-11-11 11:11.11

// telegram=|58442515747209010C1A7A8D0000000BFD0F070101046D2E06D82502FD17000082206CD825426CD0238440FF2C000F11008250FD61000082506C01018260FD6100008360FD3100000082606C01018270FD61020082706CD825|
// {"media":"smoke detector","meter":"ei6500","name":"Smokey","id":"01097274","status":"OK","last_alarm_date":"2000-01-01","alarm_counter":0,"software_version":"010107","message_datetime":"2022-05-24 06:46","duration_removed_h":0,"last_remove_date":"2000-01-01","removed_counter":0,"test_button_last_date":"2022-05-24","test_button_counter":2,"installation_date":"2022-05-24","last_sound_check_date":"2022-03-16","dust_level":"DUST_0","battery_level":"3.00V","obstacle_distance":"","timestamp":"1111-11-11T11:11:11Z"}
// |Smokey;01097274;OK;2000-01-01;0;1111-11-11 11:11.11

// telegram=|58442515747209010C1A7A900000000BFD0F070101046D3406D82502FD17000082206CD825426CD0238440FF2C020F11008250FD61010082506CD8258260FD6100008360FD3100000082606C01018270FD61020082706CD825|
// {"media":"smoke detector","meter":"ei6500","name":"Smokey","id":"01097274","status":"OK","last_alarm_date":"2022-05-24","alarm_counter":1,"software_version":"010107","message_datetime":"2022-05-24 06:52","duration_removed_h":0,"last_remove_date":"2000-01-01","removed_counter":0,"test_button_last_date":"2022-05-24","test_button_counter":2,"installation_date":"2022-05-24","last_sound_check_date":"2022-03-16","dust_level":"DUST_2","battery_level":"3.00V","obstacle_distance":"","timestamp":"1111-11-11T11:11:11Z"}
// |Smokey;01097274;OK;2022-05-24;1;1111-11-11 11:11.11

// telegram=|58442515747209010C1A7A940000000BFD0F070101046D0007D82502FD17000082206CD825426CD0238440FF2C420F11008250FD61010082506CD8258260FD6101008360FD3101000082606CD8258270FD61020082706CD825|
// {"media":"smoke detector","meter":"ei6500","name":"Smokey","id":"01097274","status":"TAMPER_WHILE_REMOVED","last_alarm_date":"2022-05-24","alarm_counter":1,"software_version":"010107","message_datetime":"2022-05-24 07:00","duration_removed_h":0.016667,"last_remove_date":"2022-05-24","removed_counter":1,"test_button_last_date":"2022-05-24","test_button_counter":2,"installation_date":"2022-05-24","last_sound_check_date":"2022-03-16","dust_level":"DUST_2","battery_level":"3.00V","obstacle_distance":"","timestamp":"1111-11-11T11:11:11Z"}
// |Smokey;01097274;TAMPER_WHILE_REMOVED;2022-05-24;1;1111-11-11 11:11.11
