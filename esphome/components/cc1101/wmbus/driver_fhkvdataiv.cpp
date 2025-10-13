/*
 Copyright (C) 2019-2022 Fredrik Öhrström (gpl-3.0-or-later)

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
        di.setName("fhkvdataiv");
        di.setDefaultFields("name,id,current_consumption_hca,set_date,consumption_at_set_date_hca,timestamp");
        di.setMeterType(MeterType::HeatCostAllocationMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_TCH,  0x08,  0x69);
        di.addDetection(MANUFACTURER_TCH,  0x08,  0x94);

        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addStringField(
            "status",
            "Meter status from tpl status field.",
            DEFAULT_PRINT_PROPERTIES  |
            PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS);

        addNumericFieldWithExtractor(
            "current_consumption",
            "The current heat cost allocation.",
             DEFAULT_PRINT_PROPERTIES,
            Quantity::HCA,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            );

        addStringFieldWithExtractor(
            "set_date",
             "The most recent billing period date.",
             DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1)));

        addNumericFieldWithExtractor(
            "consumption_at_set_date",
            "Heat cost allocation at the most recent billing period date.",
             DEFAULT_PRINT_PROPERTIES,
            Quantity::HCA,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(StorageNr(1))
            );

        addStringFieldWithExtractor(
            "set_date_1",
             "The most recent billing period date.",
             DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1)));

        addNumericFieldWithExtractor(
            "consumption_at_set_date_1",
            "Heat cost allocation at the most recent billing period date.",
             DEFAULT_PRINT_PROPERTIES,
            Quantity::HCA,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(StorageNr(1))
            );

        addStringFieldWithExtractor(
            "set_date_8",
             "The 8 billing period date.",
             DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(8)));

        addNumericFieldWithExtractor(
            "consumption_at_set_date_8",
            "Heat cost allocation at the 8 billing period date.",
             DEFAULT_PRINT_PROPERTIES,
            Quantity::HCA,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(StorageNr(8))
            );
    }
}

// Test: Rooom fhkvdataiv 14542076 FCF41938F63432975B52505F547FCEDF
// telegram=|4E4468507620541494087AAD004005089D86B62A329B3439873999738F82461ABDE3C7AC78692B363F3B41EB68607F9C9160F550769B065B6EA00A2E44346E29FF5DC5CB86283C69324AD33D137F6F|
// {"media":"heat cost allocation","meter":"fhkvdataiv","name":"Rooom","id":"14542076","status":"OK","current_consumption_hca":2,"set_date":"2020-12-31","consumption_at_set_date_hca":25,"set_date_1":"2020-12-31","consumption_at_set_date_1_hca":25,"set_date_8":"2019-10-31","consumption_at_set_date_8_hca":0,"timestamp":"1111-11-11T11:11:11Z"}
// |Rooom;14542076;2;2020-12-31;25;1111-11-11 11:11.11
