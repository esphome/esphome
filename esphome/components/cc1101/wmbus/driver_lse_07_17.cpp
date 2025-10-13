/*
 Copyright (C) 2021-2024 Fredrik Öhrström (gpl-3.0-or-later)

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
            di.setName("lse_07_17");
            di.setDefaultFields("name,id,total_m3,due_date_m3,due_date,error_code,error_date,device_date_time,timestamp");
            di.setMeterType(MeterType::WaterMeter);
            di.addLinkMode(LinkMode::S1);
            di.addDetection(MANUFACTURER_LSE, 0x06,  0x18);
            di.addDetection(MANUFACTURER_LSE, 0x07,  0x18);
            di.addDetection(MANUFACTURER_LSE, 0x07,  0x16);
            di.addDetection(MANUFACTURER_LSE, 0x07,  0x17);
            di.addDetection(MANUFACTURER_LSE, 0x07,  0xd8);
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
            "due_date",
            "The water consumption at the due date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(1))
            );

        addStringFieldWithExtractor(
            "due_date",
            "The due date configured on the meter.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "what_date",
            "The water consumption at the what date?",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(8))
            );

        addStringFieldWithExtractor(
            "what_date",
            "The what date?",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(8))
            );

        addStringFieldWithExtractorAndLookup(
            "error_code",
            "Error code of the Meter, 0 means no error.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(DifVifKey("02BB56")
            ),
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::MapType::BitToString,
                        AlwaysTrigger, MaskBits(0xffff),
                        "OK",
                        {
                            // { 0x01, "?" },
                        }
                    },
                },
            });

        addStringFieldWithExtractor(
            "error_date",
            "The date the error occurred at. If no error, reads 2127-15-31 (FFFF).",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::AtError)
            .set(VIFRange::Date)
            );

        addStringFieldWithExtractor(
            "device_date_time",
            "Date when measurement was recorded.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            );

        addStringFieldWithExtractor(
            "meter_version",
            "Meter model/version.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ModelVersion)
            );
    }
}

    /*
    The following telegram corresponds to the Qundis QWater5.5 cold water meters I have here.
    From the device display it states that it is set to S-mode operation, sending a telegram
    every 4 h.
    Another option of this device is the C mode operation, sending telegrams every
    7.5 s.

    Even though my meters are definitely Qundis QWater5.5, the meters do not identify with
    manufacturer code QDS but with LSE.

    (lse_07_17) 0f: 0C dif (8 digit BCD Instantaneous value)
    (lse_07_17) 10: 13 vif (Volume l)
    (lse_07_17) 11: * 04400100 total consumption (14.004000 m3)
    (lse_07_17) 15: 4C dif (8 digit BCD Instantaneous value storagenr=1)
    (lse_07_17) 16: 13 vif (Volume l)
    (lse_07_17) 17: * 40620000 due date consumption (6.240000 m3)
    (lse_07_17) 1b: 42 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
    (lse_07_17) 1c: 6C vif (Date type G)
    (lse_07_17) 1d: * 9F2C due date (2020-12-31)
    (lse_07_17) 1f: 02 dif (16 Bit Integer/Binary Instantaneous value)
    (lse_07_17) 20: BB vif (Volume flow l/h)
    (lse_07_17) 21: 56 vife (duration of limit exceed last lower  is 2)
    (lse_07_17) 22: * 0000 error code (0)
    (lse_07_17) 24: 32 dif (16 Bit Integer/Binary Value during error state)
    (lse_07_17) 25: 6C vif (Date type G)
    (lse_07_17) 26: * FFFF error date (2127-15-31)
    (lse_07_17) 28: 04 dif (32 Bit Integer/Binary Instantaneous value)
    (lse_07_17) 29: 6D vif (Date and time type)
    (lse_07_17) 2a: * 180DA924 device datetime (2021-04-09 13:24)

    if (findKey(MeasurementType::Instantaneous, VIFRange::DateTime, 0, 0, &key, &t->dv_entries)) {
        struct tm datetime;
        extractDVdate(&t->dv_entries, key, &offset, &datetime);
        device_date_time_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " device datetime (%s)", device_date_time_.c_str());
    }
    // How do the following error codes on the display map to the code in the telegram?
    // According to the datasheet, these errors can appear on the display:
    // LEAC Leak in the system (no associated error code)
    // 0    Negative direction of flow.
    // 2    Operating hours expired.
    // 3    Hardware error.
    // 4    Permanently stored error.
    // b    Communication via OPTO too often per month.
    // c    Communication via M-Bus too often per month.
    // d    Flow too high.
    // f    Device was without voltage supply briefly. All parameter settings are lost.
    */


// Test: Water lse_07_17 13963399 NOKEY

// telegram=|244465329933961318067AE1000000_8C04130070000082046CBE2B01FD0C11046D010CA22C|
// {"media":"warm water","meter":"lse_07_17","name":"Water","id":"13963399","what_date_m3":7,"what_date":"2021-11-30","device_date_time":"2021-12-02 12:01","meter_version":"11","timestamp":"1111-11-11T11:11:11Z"}
// |Water;13963399;null;null;null;null;null;2021-12-02 12:01;1111-11-11 11:11.11

// telegram=|2A4465329933961318067AD8800000_8C04130070000082046CBE2B01FD0C11046D1800A12C02FDAC7E9B2E|
// {"media":"warm water","meter":"lse_07_17","name":"Water","id":"13963399","what_date_m3":7,"what_date":"2021-11-30","device_date_time":"2021-12-01 00:24","meter_version":"11","timestamp":"1111-11-11T11:11:11Z"}
// |Water;13963399;null;null;null;null;null;2021-12-01 00:24;1111-11-11 11:11.11

// telegram=|2D4465329933961318067ADA000000_0C13567100004C1300000000426CFFFF02BB560000326CFFFF046D2307A12C|
// {"media":"warm water","meter":"lse_07_17","name":"Water","id":"13963399","total_m3":7.156,"due_date_m3":0,"due_date":"2127-15-31","what_date_m3":7,"what_date":"2021-11-30","error_code":"OK","error_date":"2127-15-31","device_date_time":"2021-12-01 07:35","meter_version":"11","timestamp":"1111-11-11T11:11:11Z"}
// |Water;13963399;7.156;0;2127-15-31;OK;2127-15-31;2021-12-01 07:35;1111-11-11 11:11.11

// Test: Water2 lse_07_17 09993623 NOKEY
// telegram=|2d44653223369909d8077a80000000046d130aed2B0c13233332004c1351762700426cdf2c326cffff02BB560000|
// {"device_date_time": "2023-11-13 10:19","due_date": "2022-12-31","due_date_m3": 277.651,"error_code": "OK","error_date": "2127-15-31","id": "09993623","media": "water","meter": "lse_07_17","name": "Water2","timestamp": "1111-11-11T11:11:11Z","total_m3": 323.323}
// |Water2;09993623;323.323;277.651;2022-12-31;OK;2127-15-31;2023-11-13 10:19;1111-11-11 11:11.11
