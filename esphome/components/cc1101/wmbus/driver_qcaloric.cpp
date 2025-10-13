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
        di.setName("qcaloric");
        di.addNameAlias("whe5x");
        di.addNameAlias("whe46x");
        di.setDefaultFields("name,id,current_consumption_hca,set_date,consumption_at_set_date_hca,timestamp");
        di.setMeterType(MeterType::HeatCostAllocationMeter);
        di.addLinkMode(LinkMode::C1);
        di.addLinkMode(LinkMode::T1);
        di.addLinkMode(LinkMode::S1);
        di.addDetection(MANUFACTURER_LSE, 0x08,  0x34);
        di.addDetection(MANUFACTURER_LSE, 0x08,  0x35);
        di.addDetection(MANUFACTURER_QDS, 0x08,  0x35);
        di.addDetection(MANUFACTURER_QDS, 0x08,  0x34);
        di.addDetection(MANUFACTURER_QDS, 0x08,  0x36);
        di.addDetection(MANUFACTURER_LSE, 0x08,  0x18); // whe4
        di.addDetection(MANUFACTURER_ZRI, 0x08,  0xfd);

        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addStringFieldWithExtractorAndLookup(
            "status",
            "Meter status from tpl status field.",
            DEFAULT_PRINT_PROPERTIES   |
            PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS,
            FieldMatcher::build()
            .set(DifVifKey("01FD73")),
            Translate::Lookup(
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::MapType::BitToString,
                        AlwaysTrigger, MaskBits(0xff),
                        "OK",
                        {
                            // Bits unknown
                        }
                    },
                },
            }));

        addNumericFieldWithExtractor(
            "current_consumption",
            "The current heat cost allocation.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::HCA,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::HeatCostAllocation)
            );

        addStringFieldWithExtractor(
            "set_date",
            "The most recent billing period date.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "consumption_at_set_date",
            "Heat cost allocation at the most recent billing period date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::HCA,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::HeatCostAllocation)
            .set(StorageNr(1))
            );

        addStringFieldWithExtractor(
            "set_date_1",
            "The most recent billing period date.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "consumption_at_set_date_1",
            "Heat cost allocation at the most recent billing period date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::HCA,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::HeatCostAllocation)
            .set(StorageNr(1))
            );

        addStringFieldWithExtractor(
            "set_date_8",
            "The 8 billing period date.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(8))
            );

        addNumericFieldWithExtractor(
            "consumption_at_set_date_8",
            "Heat cost allocation at the 8 billing period date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::HCA,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::HeatCostAllocation)
            .set(StorageNr(8))
            );

        addStringFieldWithExtractor(
            "set_date_17",
            "The 17 billing period date.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(17))
            );

        addNumericFieldWithExtractor(
            "consumption_at_set_date_17",
            "Heat cost allocation at the 17 billing period date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::HCA,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::HeatCostAllocation)
            .set(StorageNr(17))
            );

        addStringFieldWithExtractor(
            "error_date",
            "Date when the meter entered an error state.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::AtError)
            .set(VIFRange::Date)
            );

        addStringFieldWithExtractor(
            "device_date_time",
            "Date and time when the meter sent the telegram.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            );

        addStringFieldWithExtractor(
            "model_version",
            "model version.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ModelVersion)
            );

        addNumericFieldWithExtractor(
            "flow_temperature",
            "Forward media temperature.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::FlowTemperature)
            );

    }
}

// Test: MyElement qcaloric 78563412 NOKEY
// telegram=|314493441234567835087a740000200b6e2701004b6e450100426c5f2ccb086e790000c2086c7f21326cffff046d200b7422|
// {"media":"heat cost allocation","meter":"qcaloric","name":"MyElement","id":"78563412","status":"OK","current_consumption_hca":127,"set_date":"2018-12-31","consumption_at_set_date_hca":145,"set_date_1":"2018-12-31","consumption_at_set_date_1_hca":145,"set_date_17":"2019-01-31","consumption_at_set_date_17_hca":79,"error_date":"2127-15-31","device_date_time":"2019-02-20 11:32","timestamp":"1111-11-11T11:11:11Z"}
// |MyElement;78563412;127;2018-12-31;145;1111-11-11 11:11.11

// Test: MyElement2 qcaloric 90919293 NOKEY
// Comment: Test mostly proprietary telegram without values
// telegram=|49449344939291903408780DFF5F350082180000800007B06EFFFF970000009F2C70020000BE26970000000000010018002E001F002E0023FF210008000500020000002F046D220FA227|
// {"media":"heat cost allocation","meter":"qcaloric","name":"MyElement2","id":"90919293","status":"OK","device_date_time":"2021-07-02 15:34","timestamp":"1111-11-11T11:11:11Z"}
// |MyElement2;90919293;null;null;null;1111-11-11 11:11.11

// Test: zenner_heat qcaloric 25932395 NOKEY
// telegram=|5E44496A95239325FD087A2CC050052F2F_0B6E030100426CDF2C4B6EFFFFFF82046CE1228B046E6200008D04EE132C3BFEFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF2F2F2F2F|
// {"media":"heat cost allocation","meter":"qcaloric","name":"zenner_heat","id":"25932395","status":"UNKNOWN_C0","current_consumption_hca":103,"set_date":"2022-12-31","set_date_1":"2022-12-31","set_date_8":"2023-02-01","consumption_at_set_date_8_hca":62,"timestamp":"1111-11-11T11:11:11Z"}
// |zenner_heat;25932395;103;2022-12-31;null;1111-11-11 11:11.11

// Comment: Normal telegram that fills in values.
// telegram=|314493449392919034087a520000200b6e9700004b6e700200426c9f2ccb086e970000c2086cbe26326cffff046d2d16a227|
// {"media":"heat cost allocation","meter":"qcaloric","name":"MyElement2","id":"90919293","status":"OK","current_consumption_hca":97,"set_date":"2020-12-31","consumption_at_set_date_hca":270,"set_date_1":"2020-12-31","consumption_at_set_date_1_hca":270,"set_date_17":"2021-06-30","consumption_at_set_date_17_hca":97,"error_date":"2127-15-31","device_date_time":"2021-07-02 22:45","timestamp":"1111-11-11T11:11:11Z"}
// |MyElement2;90919293;97;2020-12-31;270;1111-11-11 11:11.11

// Comment: Another mostly empty telegram, but values are now valid.
// telegram=|49449344939291903408780DFF5F350082180000800007B06EFFFF970000009F2C70020000BE26970000000000010018002E001F002E0023FF210008000500020000002F046D220FA228|
// {"media":"heat cost allocation","meter":"qcaloric","name":"MyElement2","id":"90919293","status":"OK","current_consumption_hca":97,"set_date":"2020-12-31","consumption_at_set_date_hca":270,"set_date_1":"2020-12-31","consumption_at_set_date_1_hca":270,"set_date_17":"2021-06-30","consumption_at_set_date_17_hca":97,"error_date":"2127-15-31","device_date_time":"2021-08-02 15:34","timestamp":"1111-11-11T11:11:11Z"}
// |MyElement2;90919293;97;2020-12-31;270;1111-11-11 11:11.11

// Comment: Another version of the heat cost allocator. Was known as whe5x, so a name alias exist that maps to qcaloric.
// Test: HCA whe5x 91835132 NOKEY
// telegram=|244465323251839134087a4f0000000b6e0403004b6e660300426c9e29326cffff046d1416b921dd2f|
// {"media":"heat cost allocation","meter":"qcaloric","name":"HCA","id":"91835132","status":"OK","current_consumption_hca":304,"set_date":"2020-09-30","consumption_at_set_date_hca":366,"set_date_1":"2020-09-30","consumption_at_set_date_1_hca":366,"error_date":"2127-15-31","device_date_time":"2021-01-25 22:20","timestamp":"1111-11-11T11:11:11Z"}
// |HCA;91835132;304;2020-09-30;366;1111-11-11 11:11.11

// Comment: Another version of the heat cost allocator. Was known as whe46x, which now is a name alias mapped to qcaloric.
// Test: HCA2 whe46x 60366655 NOKEY
// telegram=|344465325566366018087A90040000046D1311962C01FD0C03326CFFFF01FD7300025AC2000DFF5F0C0008003030810613080BFFFC|
// {"media":"heat cost allocation","meter":"qcaloric","name":"HCA2","id":"60366655","status":"POWER_LOW","error_date":"2127-15-31","device_date_time":"2020-12-22 17:19","model_version":"03","flow_temperature_c":19.4,"timestamp":"1111-11-11T11:11:11Z"}
// |HCA2;60366655;null;null;null;1111-11-11 11:11.11

// telegram=|2a4465325566366018087ac3040000046d1617Ba210B6e890000426c9f2c4B6e520600326cffff01fd7300|
// {"media":"heat cost allocation","meter":"qcaloric","name":"HCA2","id":"60366655","status":"POWER_LOW","current_consumption_hca":89,"set_date":"2020-12-31","consumption_at_set_date_hca":652,"set_date_1":"2020-12-31","consumption_at_set_date_1_hca":652,"error_date":"2127-15-31","device_date_time":"2021-01-26 23:22","model_version":"03","flow_temperature_c":19.4,"timestamp":"1111-11-11T11:11:11Z"}
// |HCA2;60366655;89;2020-12-31;652;1111-11-11 11:11.11

// Test: HCA55 qcaloric 30535282 NOKEY
// Comment: QCaloric 5.5 encrypted but with some not-encrypted bytes at the end. We should print these, but we do not right now....
// Since it is encrypted and we do not have the key, wmbusmeters currently ignores it and every following telegram.
// The readable bytes are: 326cffff046d230dda2c which decodes to
// 32 dif (16 Bit Integer/Binary Value during error state)
// 6C vif (Date type G)
// FFFF
// 04 dif (32 Bit Integer/Binary Instantaneous value)
// 6D vif (Date and time type)
// 230DDA2C
// 32 dif (16 Bit Integer/Binary Value during error state)
// 6C vif (Date type G)
// FFFF ("error_date":"2127-15-31")
// 04 dif (32 Bit Integer/Binary Instantaneous value)
// 6D vif (Date and time type)
// 230DDA2C ("device_date_time":"2022-12-26 13:35")
// NOTYET telegram=|384493448252533036087A430020253F59515BD90F76E8576AF36C988EEA9B398EC5C205E5DBBE3F2698408947CB8E326CFFFF046D230DDA2C|

// Comment: Mostly mfct specific data. Not yet decoded.
// telegram=|49449344825253303608780DFF5F350082430035E3DFC4EAC97A58B8610713D93549E2601258D617D267E7515C764B002A88CD341A9F9DF3C6034DE5B6D1FAB3619CBA9F046D250DDA2C|
// {"device_date_time": "2022-12-26 13:37","id": "30535282","media": "heat cost allocation","meter": "qcaloric","name": "HCA55","status": "OK","timestamp": "1111-11-11T11:11:11Z"}
// |HCA55;30535282;null;null;null;1111-11-11 11:11.11
