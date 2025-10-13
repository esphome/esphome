/*
 Copyright (C) 2017-2022 Fredrik Öhrström (gpl-3.0-or-later)
 Copyright (C)      2020 Avandorp (gpl-3.0-or-later)

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

/*
 AquaMetro / Integra water meter "TOPAS ES KR"
 Models TOPAS ES KR 95077 95056 95345 95490 95373 95059 95065 95068 95071 95074 should be compatible. Only 95059 in one configuration tested.
 Identifies itself with Manufacturer "AMT" and Version "f1"
 Product leaflet and observation says the following values are sent:
 Current total volume
 Total volume at end of year-period day (that means: current total volume - total volume at end of year-period day = current year-periods volume up until now)
 Total backward volume on end of year-period day or total backward volume in current year-period. Backward volume remains untested (luckily only 0 values encountered).
 Date of end of last year-period day
 Total volume at end of last month-period dateTime
 DateTime of end of last month-period
 Current flow rate
 Battery life (days left)
 Water temperature

 Example telegram:
 telegram=|4E44B40512345678F1077A310040052F2F|01FD08040C13991848004C1359423500CC101300000000CC201359423500426C7F2C0B3B00000002FD74DA10025AD300C4016D3B179F27CC011387124600|+2
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
        di.setName("topaseskr");
        di.setDefaultFields(
            "name,id,total_m3,temperature_c,current_flow_m3h,volume_year_period_m3,"
            "reverse_volume_year_period_m3,meter_year_period_end_date,volume_month_period_m3,"
            "meter_month_period_end_datetime,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_AMT, 0x06,  0xf1);
        di.addDetection(MANUFACTURER_AMT, 0x07,  0xf1);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addOptionalLibraryFields("total_m3,access_counter");

        addNumericFieldWithExtractor(
            "temperature",
            "Current water temperature recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::FlowTemperature));

        addNumericFieldWithExtractor(
            "current_flow",
            "The current water flow.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Flow,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::VolumeFlow));

        addNumericFieldWithExtractor(
            "volume_year_period",
            "Volume up to end of last year-period.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "reverse_volume_year_period",
            "Reverse volume in this year-period (?)",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(1))
            .set(TariffNr(1))
            );


        addStringFieldWithExtractor(
            "meter_year_period_end_date",
            "Meter date for year-period end.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "volume_month_period",
            "Volume up to end of last month-period.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(3))
            );

        addStringFieldWithExtractor(
            "meter_month_period_end_datetime",
            "Meter timestamp for month-period end.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            .set(StorageNr(3))
            );

        addNumericFieldWithExtractor(
            "battery",
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

// Test: Witer topaseskr 78563412 NOKEY
// telegram=|4E44B40512345678F1077A310040052F2F_01FD08040C13991848004C1359423500CC101300000000CC201359423500426C7F2C0B3B00000002FD74DA10025AD300C4016D3B179F27CC011387124600|
// {"media":"water","meter":"topaseskr","name":"Witer","id":"78563412","total_m3":481.899,"access_counter":4,"temperature_c":21.1,"current_flow_m3h":0,"volume_year_period_m3":354.259,"reverse_volume_year_period_m3":0,"meter_year_period_end_date":"2019-12-31","volume_month_period_m3":461.287,"meter_month_period_end_datetime":"2020-07-31 23:59","battery_y":11.811331,"timestamp":"1111-11-11T11:11:11Z"}
// |Witer;78563412;481.899;21.1;0;354.259;0;2019-12-31;461.287;2020-07-31 23:59;1111-11-11 11:11.11

// Test: Woter topaseskr 69190253 NOKEY
// telegram=|4E44B40553021969F1077A0C0040052F2F_01FD08800C13914544004C1393673500CC101300000000CC201393673500426CDF2C0B3B0100F002FD747912025AAE00C4016D3B17FF25CC011325584100|
// {"access_counter": 128,"battery_y": 12.947562,"current_flow_m3h": -0.001,"id": "69190253","media": "water","meter": "topaseskr","meter_month_period_end_datetime": "2023-05-31 23:59","meter_year_period_end_date": "2022-12-31","name": "Woter","reverse_volume_year_period_m3": 0,"temperature_c": 17.4,"timestamp": "1111-11-11T11:11:11Z","total_m3": 444.591,"volume_month_period_m3": 415.825,"volume_year_period_m3": 356.793}
// |Woter;69190253;444.591;17.4;-0.001;356.793;0;2022-12-31;415.825;2023-05-31 23:59;1111-11-11 11:11.11
