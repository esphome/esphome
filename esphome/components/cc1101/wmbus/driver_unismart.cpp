/*
 Copyright (C) 2021-2022 Fredrik Öhrström (gpl-3.0-or-later)

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
        di.setName("unismart");
        di.setDefaultFields("name,id,total_m3,target_m3,timestamp");
        di.setMeterType(MeterType::GasMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_AMX, 0x03,  0x01);

        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addOptionalLibraryFields("fabrication_no");

        addStringFieldWithExtractorAndLookup(
            "status",
            "Status of meter?",
            DEFAULT_PRINT_PROPERTIES  | PrintProperty::STATUS,
            FieldMatcher::build()
            .set(DifVifKey("02FD74")),
            {
                {
                    {
                        "STATUS_FLAGS",
                        Translate::MapType::BitToString,
                        AlwaysTrigger, MaskBits(0xffff),
                        "OK",
                        {
                        }
                    },
                },
            });

        addStringFieldWithExtractorAndLookup(
            "other",
            "Other status of meter?",
            DEFAULT_PRINT_PROPERTIES  | PrintProperty::STATUS,
            FieldMatcher::build()
            .set(DifVifKey("017F")),
            {
                {
                    {
                        "OTHER_FLAGS",
                        Translate::MapType::BitToString,
                        AlwaysTrigger, MaskBits(0xff),
                        "",
                        {
                        }
                    },
                },
            });

        addStringFieldWithExtractor(
            "total_date_time",
            "Timestamp for this total measurement.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            .set(IndexNr(1))
            );

        addNumericFieldWithExtractor(
            "total",
             "The total gas consumption recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .add(VIFCombinable::UncorrectedMeterUnit)
            );

        addStringFieldWithExtractor(
            "target_date_time",
            "Timestamp for gas consumption recorded at the beginning of this month.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "target",
             "The total gas consumption recorded by this meter at the beginning of this month.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(1))
            .add(VIFCombinable::UncorrectedMeterUnit)
            );

        addStringFieldWithExtractor(
            "version",
            "Model version.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ModelVersion)
            );

        addStringFieldWithExtractor(
            "supplier_info",
            "Supplier info?",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::SpecialSupplierInformation)
            );

        addStringFieldWithExtractor(
            "parameter_set",
            "Meter configued with this parameter set?",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ParameterSet)
            );

        addStringFieldWithExtractor(
            "meter_timestamp",
            "Timestamp when this measurement was sent.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            .set(IndexNr(2))
            );

    }
}

// Test: GasMeter unismart 00043094 00000000000000000000000000000000
// telegram=|6044B8059430040001037A1D005085E2B670BCF1A5C87E0C1A51DA18924EF984613DA2A9CD39D8F4C7208326C76D42DBEADF80D574192B71BD7C4F56A7F1513151768A9DB804883B28CB085CA2D0F7438C361CB9E2734712ED9BFBB2A14EF55208|
// {"media":"gas","meter":"unismart","name":"GasMeter","id":"00043094","fabrication_no":"03162296","status":"STATUS_FLAGS_CF0","other":"OTHER_FLAGS_14","total_date_time":"2021-09-15 13:18","total_m3":917,"target_date_time":"2021-09-01 06:00","target_m3":911.32,"version":"  4GGU","supplier_info":"00","parameter_set":"02","meter_timestamp":"2021-09-15 13:18:30","timestamp":"1111-11-11T11:11:11Z"}
// |GasMeter;00043094;917;911.32;1111-11-11 11:11.11
