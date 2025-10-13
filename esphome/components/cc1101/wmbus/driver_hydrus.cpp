/*
 Copyright (C) 2019-2023 Fredrik Öhrström (gpl-3.0-or-later)
 Copyright (C)      2021 Vincent Privat (gpl-3.0-or-later)

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
        di.setName("hydrus");
        di.setDefaultFields("name,id,total_m3,total_at_date_m3,status,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_DME,  0x07,  0x70);
        di.addDetection(MANUFACTURER_DME,  0x07,  0x76);
        di.addDetection(MANUFACTURER_HYD,  0x07,  0x24);
        di.addDetection(MANUFACTURER_HYD,  0x07,  0x8b);
        di.addDetection(MANUFACTURER_HYD,  0x06,  0x8b);
        di.addDetection(MANUFACTURER_DME,  0x06,  0x70);
        di.addDetection(MANUFACTURER_DME,  0x16,  0x70);

        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addOptionalLibraryFields("operating_time_h,actuality_duration_s,meter_datetime,customer");
        addOptionalLibraryFields("flow_temperature_c,external_temperature_c");

        addStringField(
            "status",
            "Status of meter.",
            PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS);

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
            "total_tariff{tariff_counter}",
            "The total water consumption recorded on tariff # by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(TariffNr(1),TariffNr(2))
            );

        addNumericFieldWithExtractor(
            "total_tariff{tariff_counter}_at_date",
            "The total water consumption recorded on tariff # by this meter at billing date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(TariffNr(1),TariffNr(2))
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "flow",
            "The current water flow.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Flow,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::VolumeFlow));

        addNumericFieldWithExtractor(
            "total_at_date",
            "The total water consumption recorded at date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "at",
            "The last billing period date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::PointInTime,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1)),
            Unit::DateLT
            );

        addNumericFieldWithExtractor(
            "target",
            "The total water consumption recorded at the end of last month.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(3))
            );

        addNumericFieldWithExtractor(
            "target",
            "The end of last month.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::PointInTime,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            .set(StorageNr(3))
            );

        addNumericFieldWithExtractor(
            "remaining_battery_life",
            "Remaining battery life in years.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Time,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::RemainingBattery),
            Unit::Year);
    }
}

// Test: HydrusWater hydrus 64646464 NOKEY
// Comment:
// telegram=|4E44A5116464646470077AED004005_2F2F01FD08300C13741100007C1300000000FC101300000000FC201300000000726C00000B3B00000002FD748713025A6800C4016D3B177F2ACC011300020000|
// {"media":"water","meter":"hydrus","name":"HydrusWater","id":"64646464","total_m3":1.174,"flow_m3h":0,"flow_temperature_c":10.4,"remaining_battery_life_y":13.686797,"status":"OK","target_datetime":"2019-10-31 23:59","target_m3": 0.2,"timestamp":"1111-11-11T11:11:11Z"}
// |HydrusWater;64646464;1.174;null;OK;1111-11-11 11:11.11

// Test: HydrusVater hydrus 65656565 NOKEY
// Comment:
// telegram=|3E44A5116565656570067AFB0030052F2F_0C13503400000DFD110A383731303134423032410B3B00000002FD74DC15C4016D3B178D29CC0113313400002F2F|
// {"media":"warm water","meter":"hydrus","name":"HydrusVater","id":"65656565","flow_m3h":0,"customer": "A20B410178","total_m3":3.45,"remaining_battery_life_y":15.321328,"target_datetime":"2020-09-13 23:59","target_m3": 3.431,"status":"OK","timestamp":"1111-11-11T11:11:11Z"}
// |HydrusVater;65656565;3.45;null;OK;1111-11-11 11:11.11

// Test: HydrusAES hydrus 64745666 NOKEY
// Comment:
// telegram=||6644242328001081640E7266567464A51170071F0050052C411A08674048DD6BA82A0DF79FFD401309179A893A1BE3CE8EDC50C2A45CD7AFEC3B4CE765820BE8056C124A17416C3722985FFFF7FCEB7094901AB3A16294B511B9A740C9F9911352B42A72FB3B0C|
// {"media":"water","meter":"hydrus","name":"HydrusAES","id":"64745666","total_m3":137.291,"total_tariff1_m3":0,"total_tariff2_m3":137.291,"flow_m3h":0,"flow_temperature_c":24.5,"external_temperature_c":23.9,"total_at_date_m3":128.638,"total_tariff1_at_date_m3":0,"total_tariff2_at_date_m3":128.638,"at_date":"2020-12-31","actuality_duration_s":6673,"operating_time_h":14678,"meter_datetime": "2021-01-23 08:27","status":"OK","timestamp":"1111-11-11T11:11:11Z"}
// |HydrusAES;64745666;137.291;128.638;OK;1111-11-11 11:11.11

// Test: HydrusIzarRS hydrus 60897379 NOKEY
// Comment: Hydrus IZAR RS 868 water meter
// telegram=|1E4424238B07797389607A8F00107D_041312170100426CBF23441344100100|
// {"media":"water","meter":"hydrus","name":"HydrusIzarRS","id":"60897379","total_m3":71.442,"at_date": "2021-03-31","total_at_date_m3":69.7,"status":"OK","timestamp":"1111-11-11T11:11:11Z"}
// |HydrusIzarRS;60897379;71.442;69.7;OK;1111-11-11 11:11.11

// Test: HydrusIzarRSWarm hydrus 60904720 NOKEY
// Comment: Hydrus IZAR RS 868 water meter warm
// telegram=|1E4424238B06204790607A2A0010D8_0413DDC00000426CBF23441382BB0000|
// {"media":"warm water","meter":"hydrus","name":"HydrusIzarRSWarm","id":"60904720","total_m3":49.373,"total_at_date_m3":48.002,"at_date":"2021-03-31","status":"OK","timestamp":"1111-11-11T11:11:11Z"}
// |HydrusIzarRSWarm;60904720;49.373;48.002;OK;1111-11-11 11:11.11

// Test: HydrusFoo hydrus 64641820 NOKEY
// Comment: Negative power values.
// telegram=|6344A5112018646470078C00D7900F002C256AB59B00F0F13032019092DE7A6A004007102F2F0C13896729004C1323462400CC101300000000CC201323462400426CDF2C0B3B0200F002FD742F0D025AC100C4016D3B17FE29CC01132841290001FD089F|
// {"at_date": "2022-12-31","target_datetime": "2023-09-30 23:59","flow_m3h": -0.002,"flow_temperature_c": 19.3,"id": "64641820","media": "water","meter": "hydrus","name": "HydrusFoo","remaining_battery_life_y": 9.240436,"status": "OK","timestamp": "1111-11-11T11:11:11Z","total_at_date_m3":244.623,"target_m3": 294.128,"total_m3": 296.789,"total_tariff1_at_date_m3": 0,"total_tariff2_at_date_m3": 244.623}
// |HydrusFoo;64641820;296.789;244.623;OK;1111-11-11 11:11.11
