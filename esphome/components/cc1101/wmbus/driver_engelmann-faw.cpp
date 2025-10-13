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
        di.setName("engelmann-faw");
        di.setDefaultFields("name,id,status,reporting_date,consumption_at_reporting_date_m3,timestamp");
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_EFE,  0x07,  0x00);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) :
        MeterCommonImplementation(mi, di)
    {
        addStringFieldWithExtractorAndLookup(
            "status",
            "Status and error flags.",
            DEFAULT_PRINT_PROPERTIES  | PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS,
            FieldMatcher::build()
            .set(VIFRange::ErrorFlags),
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::MapType::BitToString,
                        AlwaysTrigger, MaskBits(0xff),
                        "OK",
                        {
                            { 0x01, "VOLUME_DETECTION_COILS_DEFECT" },
                            { 0x02, "RESET" },
                            { 0x04, "CRC_ERROR" },
                            { 0x08, "REMOVAL_DETECTED" },
                            { 0x10, "MAGNETIC_MANIPULATION" },
                            { 0x20, "LEAKAGE" },
                            { 0x40, "BLOCKED" },
                            { 0x80, "REVERSE_FLOW" },
                        }
                    },
                },
            });

        addStringFieldWithExtractor(
            "reporting_date",
            "The reporting date of the last billing period.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "consumption_at_reporting_date",
            "The water consumption at the last billing period date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(1)));

        for (int i=2; i<=16; ++i)
        {
            string name, info;
            strprintf(&name, "consumption_%d_months_ago", i-1);
            strprintf(&info, "Water consumption %d month(s) ago.", i-1);

            addNumericFieldWithExtractor(
                name,
                info,
                DEFAULT_PRINT_PROPERTIES,
                Quantity::Volume,
                VifScaling::Auto, DifSignedness::Signed,
                FieldMatcher::build()
                .set(MeasurementType::Instantaneous)
                .set(VIFRange::Volume)
                .set(StorageNr(i)));
        }
    }
}


// Test: Wasserzaehler engelmann-faw 43000255 NOKEY
// telegram=|8f44c5145502004301077260402520c51400076b0000002f2f426cbf2c441322e9000001fd17008401133c340100c40113ae2d010084021303290100c402137e21010084031313180100c403138a0e010084041337060100c40413b2fc00008405139af30000c4051322e90000840613c1df0000c40613cdd5000084071365ce0000c407136dc500008408138dbf0000|
// {"media":"water","meter":"engelmann-faw","name":"Wasserzaehler","id":"20254060","status":"OK","reporting_date":"2021-12-31","consumption_at_reporting_date_m3":59.682,"consumption_1_months_ago_m3":78.908,"consumption_2_months_ago_m3":77.23,"consumption_3_months_ago_m3":76.035,"consumption_4_months_ago_m3":74.11,"consumption_5_months_ago_m3":71.699,"consumption_6_months_ago_m3":69.258,"consumption_7_months_ago_m3":67.127,"consumption_8_months_ago_m3":64.69,"consumption_9_months_ago_m3":62.362,"consumption_10_months_ago_m3":59.682,"consumption_11_months_ago_m3":57.281,"consumption_12_months_ago_m3":54.733,"consumption_13_months_ago_m3":52.837,"consumption_14_months_ago_m3":50.541,"consumption_15_months_ago_m3":49.037,"timestamp":"1111-11-11T11:11:11Z"}
// |Wasserzaehler;20254060;OK;2021-12-31;59.682;1111-11-11 11:11.11
