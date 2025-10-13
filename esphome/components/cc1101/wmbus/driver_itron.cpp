/*
 Copyright (C) 2022-2023 Fredrik Öhrström (gpl-3.0-or-later)

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
        di.setName("itron");
        di.setDefaultFields("name,id,total_m3,target_m3,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_ITW,  0x07,  0x00);
        di.addDetection(MANUFACTURER_ITW,  0x07,  0x03);
        di.addDetection(MANUFACTURER_ITW,  0x07,  0x33);
        di.addDetection(MANUFACTURER_ITW,  0x16,  0x00);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        setMeterType(MeterType::WaterMeter);

        setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

        addLinkMode(LinkMode::T1);

        addOptionalLibraryFields("enhanced_id,meter_datetime");
        addOptionalLibraryFields("total_m3,total_backward_m3,volume_flow_m3h");

        addStringFieldWithExtractorAndLookup(
            "status",
            "Status and error flags.",
            DEFAULT_PRINT_PROPERTIES  | PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ErrorFlags)
            .add(VIFCombinable::RecordErrorCodeMeterToController),
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::MapType::BitToString,
                        AlwaysTrigger, MaskBits(0xffffff),
                        "OK",
                        {
                            // No known layout for field
                        }
                    },
                },
            });

        addNumericFieldWithExtractor(
            "target",
             "The total water consumption recorded at the end of previous billing period.",
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
            "Date when previous billing period ended.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1))
            );

        addStringFieldWithExtractorAndLookup(
            "unknown_a",
            "Unknown flags.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(DifVifKey("047F")),
            {
                {
                    {
                        "WOOTA",
                        Translate::MapType::BitToString,
                        AlwaysTrigger, MaskBits(0xffffffff),
                        "",
                        {
                            // No known layout for field
                        }
                    },
                },
            });

        addStringFieldWithExtractorAndLookup(
            "unknown_b",
            "Unknown flags.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(DifVifKey("027F")),
            {
                {
                    {
                        "WOOTB",
                        Translate::MapType::BitToString,
                        AlwaysTrigger, MaskBits(0xffff),
                        "",
                        {
                            // No known layout for field
                        }
                    },
                },
            });

    }
}

// Test: SomeWater itron 12345698 NOKEY
// Comment: Test ITRON T1 telegram not encrypted, which has no 2f2f markers.
// telegram=|384497269856341203077AD90000A0#0413FD110000066D2C1AA1D521004413300F0000426CBF2C047F0000060C027F862A0E79678372082100|
// {"media":"water","meter":"itron","name":"SomeWater","id":"12345698","enhanced_id":"002108728367","meter_datetime":"2022-01-21 01:26:44","total_m3":4.605,"status":"OK","target_m3":3.888,"target_date":"2021-12-31","unknown_a":"WOOTA_C060000","unknown_b":"WOOTB_2A86","timestamp":"1111-11-11T11:11:11Z"}
// |SomeWater;12345698;4.605;3.888;1111-11-11 11:11.11

// Test: MoreWater itron 18000056 NOKEY
// telegram=|46449726560000183307725600001897263307AF0030052F2F_066D0E1015C82A000C13771252000C933C000000000B3B0400004C1361045200426CC12A03FD971C0000002F2F2F|
// {"media":"water","meter":"itron","name":"MoreWater","id":"18000056","meter_datetime":"2022-10-08 21:16:14","total_m3":521.277,"total_backward_m3":0,"volume_flow_m3h":0.004,"status":"OK","target_m3":520.461,"target_date":"2022-10-01","timestamp":"1111-11-11T11:11:11Z"}
// |MoreWater;18000056;521.277;520.461;1111-11-11 11:11.11

// Test: AnyWater itron 20310959 NOKEY
// telegram=|384497265909312000077a930000a0041360B50100066d101295f427004413ac570100426cdf2c047f0000060c027f6c2a0e79000000000000|
// {"enhanced_id": "000000000000","id": "20310959","media": "water","meter": "itron","meter_datetime": "2023-07-20 21:18:16","name": "AnyWater","status": "OK","target_date": "2022-12-31","target_m3": 87.98,"timestamp": "1111-11-11T11:11:11Z","total_m3": 111.968,"unknown_a": "WOOTA_C060000","unknown_b": "WOOTB_2A6C"}
// |AnyWater;20310959;111.968;87.98;1111-11-11 11:11.11

// Test: ColdWaterMeter itron 23362098 NOKEY
// Comment: Allmess cold water with Itron Module programmed with type 0x16
// telegram=|3A4497269820362300167AF60020A52F2F_04132E100000066D03260DE12B007413FEFEFEFE426C1F01047F1600060C027F9A2A0E79187103002300|
// {"enhanced_id": "002300037118", "id": "23362098", "media": "cold water", "meter": "itron", "meter_datetime": "2023-11-01 13:38:03", "name": "ColdWaterMeter", "status": "OK", "target_date": "2000-01-31", "timestamp": "1111-11-11T11:11:11Z", "total_m3": 4.142,"unknown_a": "WOOTA_C060016","unknown_b": "WOOTB_2A9A" }
// |ColdWaterMeter;23362098;4.142;null;1111-11-11 11:11.11
