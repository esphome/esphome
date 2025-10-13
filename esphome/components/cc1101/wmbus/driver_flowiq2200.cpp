/*
 Copyright (C) 2017-2022 Fredrik Öhrström (gpl-3.0-or-later)

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
    struct Driver : public virtual MeterCommonImplementation {
        Driver(MeterInfo &mi, DriverInfo &di);
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("flowiq2200");
        di.setDefaultFields("name,id,status,total_m3,target_m3,timestamp");

        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::C1);
        // FlowIQ2200
        di.addDetection(MANUFACTURER_KAW,  0x16,  0x3a);
	di.addDetection(MANUFACTURER_KAW,  0x16,  0x3c);
        // FlowIQ3100
        di.addDetection(MANUFACTURER_KAM,  0x16,  0x1d);

        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addStringFieldWithExtractorAndLookup(
            "status",
            "Status of meter. Not fully understood!",
            DEFAULT_PRINT_PROPERTIES | PrintProperty::STATUS,
            FieldMatcher::build()
            .set(DifVifKey("04FF23")),
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::MapType::BitToString,
                        AlwaysTrigger, MaskBits(0xffffffff),
                        "OK",
                        {
                            /* Maybe these are the same as the multical21 but we do not know!
                               And there are more bits here. */
                            { 0x01 , "DRY" },
                            { 0x02 , "REVERSE" },
                            { 0x04 , "LEAK" },
                            { 0x08 , "BURST" },
                        }
                    },
                },
            });

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
            "target",
             "The total water consumption recorded at the beginning of this month.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(1))
            );

        addStringFieldWithExtractor(
            "target_date",
             "The date at the beginning of this month.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "flow",
            "The current flow of water through the meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Flow,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::VolumeFlow)
            );

        addNumericFieldWithExtractor(
            "min_flow_temperature",
             "The water temperature.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Minimum)
            .set(VIFRange::FlowTemperature)
            .set(StorageNr(2))
            );

        addNumericFieldWithExtractor(
            "max_flow_temperature",
            "The maximum water temperature.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Maximum)
            .set(VIFRange::FlowTemperature)
            .set(StorageNr(2))
            );

        addNumericFieldWithExtractor(
            "min_external_temperature",
            "The external temperature outside of the meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Minimum)
            .set(VIFRange::ExternalTemperature)
            .set(StorageNr(2))
            );

        addNumericFieldWithExtractor(
            "max_flow",
            "The maximum flow recorded during previous period.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Flow,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Maximum)
            .set(VIFRange::VolumeFlow)
            .set(StorageNr(2))
            );

        addNumericFieldWithExtractor(
            "min_flow",
            "The minimum flow recorded during previous period.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Flow,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Minimum)
            .set(VIFRange::VolumeFlow)
            .set(StorageNr(2))
            );

        addNumericFieldWithExtractor(
            "max_external_temperature",
            "The maximum temperature recorded during previous period.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Maximum)
            .set(VIFRange::ExternalTemperature)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "min_external_temperature",
            "The minimum flow recorded during previous period.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Minimum)
            .set(VIFRange::ExternalTemperature)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "max_flow",
            "The maximum flow recorded during previous period.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Flow,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Maximum)
            .set(VIFRange::VolumeFlow)
            .set(StorageNr(1))
            );

        addStringFieldWithExtractorAndLookup(
            "time_dry",
            "Amount of time the meter has been dry.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(DifVifKey("02FF20")),
            {
                {
                    {
                        "DRY",
                        Translate::MapType::IndexToString,
                        AlwaysTrigger, MaskBits(0x0070),
                        "",
                        {
                            { 0x0000, "" },
                            { 0x0010, "1-8 hours" },
                            { 0x0020, "9-24 hours" },
                            { 0x0030, "2-3 days" },
                            { 0x0040, "4-7 days" },
                            { 0x0050, "8-14 days" },
                            { 0x0060, "15-21 days" },
                            { 0x0070, "22-31 days" },
                        },
                    },
                },
            });

        addStringFieldWithExtractorAndLookup(
            "time_reversed",
            "Amount of time the meter has been reversed.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(DifVifKey("02FF20")),
            {
                {
                    {
                        "REVERSED",
                        Translate::MapType::IndexToString,
                        AlwaysTrigger, MaskBits(0x0380),
                        "",
                        {
                            { 0x0000, "" },
                            { 0x0080, "1-8 hours" },
                            { 0x0100, "9-24 hours" },
                            { 0x0180, "2-3 days" },
                            { 0x0200, "4-7 days" },
                            { 0x0280, "8-14 days" },
                            { 0x0300, "15-21 days" },
                            { 0x0380, "22-31 days" },
                        },
                    },
                },
            });

        addStringFieldWithExtractorAndLookup(
            "time_leaking",
            "Amount of time the meter has been leaking.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(DifVifKey("02FF20")),
            {
                {
                    {
                        "LEAKING",
                        Translate::MapType::IndexToString,
                        AlwaysTrigger, MaskBits(0x1c00),
                        "",
                        {
                            { 0x0000, "" },
                            { 0x0400, "1-8 hours" },
                            { 0x0800, "9-24 hours" },
                            { 0x0c00, "2-3 days" },
                            { 0x1000, "4-7 days" },
                            { 0x1400, "8-14 days" },
                            { 0x1800, "15-21 days" },
                            { 0x1c00, "22-31 days" },
                        },
                    },
                },
            });

        addStringFieldWithExtractorAndLookup(
            "time_bursting",
            "Amount of time the meter has been bursting.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(DifVifKey("02FF20")),
            {
                {
                    {
                        "BURSTING",
                        Translate::MapType::IndexToString,
                        AlwaysTrigger, MaskBits(0xe000),
                        "",
                        {
                            { 0x0000, "" },
                            { 0x2000, "1-8 hours" },
                            { 0x4000, "9-24 hours" },
                            { 0x6000, "2-3 days" },
                            { 0x8000, "4-7 days" },
                            { 0xa000, "8-14 days" },
                            { 0xc000, "15-21 days" },
                            { 0xe000, "22-31 days" },
                        },
                    },
                },
            });

    }
}

// Test: VATTEN flowiq2200 52525252 NOKEY
// telegram=|4D44372C525252523A168D203894DF7920F93278_04FF23000000000413AEAC0000441364A80000426C812A023B000092013BEF01A2013B000006FF1B067000097000A1015B0C91015B14A1016713|
// {"media":"cold water","meter":"flowiq2200","name":"VATTEN","id":"52525252","status":"OK","total_m3":44.206,"target_m3":43.108,"target_date":"2020-10-01","flow_m3h":0,"min_flow_temperature_c":12,"max_flow_temperature_c":20,"min_external_temperature_c":19,"max_flow_m3h":0.495,"min_flow_m3h":0,"timestamp":"1111-11-11T11:11:11Z"}
// |VATTEN;52525252;OK;44.206;43.108;1111-11-11 11:11.11

// Test: Votten flowiq2200 23813076 NOKEY
// telegram=|4744372C763081233C168D2056D1ED11205A3C78_04FF2300080000441300000000523B000006FF1B08900008F0FF426CC12B61670A51671B023B000004131F0F00008101E7FF0F13|
// {"flow_m3h": 0,"id":"23813076","max_external_temperature_c": 27,"max_flow_m3h": 0,"media":"cold water","meter":"flowiq2200","min_external_temperature_c": 10,"name":"Votten","status":"ERROR_FLAGS_800","target_date":"2022-11-01","target_m3": 0,"timestamp":"1111-11-11T11:11:11Z","total_m3": 3.871}
// |Votten;23813076;ERROR_FLAGS_800;3.871;0;1111-11-11 11:11.11
// telegram=|3244372C763081233C168D2057D2ED11205a817905095480_0008000000000000000008900008F0FFC12B0A1B23001F0F000013|
// {"media":"cold water","meter":"flowiq2200","name":"Votten","id":"23813076","status":"ERROR_FLAGS_800","total_m3":3.871,"target_m3":0,"target_date":"2022-11-01","flow_m3h":0.035,"max_external_temperature_c":27,"min_external_temperature_c":10,"max_flow_m3h":0,"timestamp":"1111-11-11T11:11:11Z"}
// |Votten;23813076;ERROR_FLAGS_800;3.871;0;1111-11-11 11:11.11
