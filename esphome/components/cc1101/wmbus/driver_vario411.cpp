/*
 Copyright (C) 2022 Fredrik Öhrström (gpl-3.0-or-later)

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
        di.setName("vario411");
        di.setDefaultFields("name,id,target_kwh,target_date,timestamp");
        di.setMeterType(MeterType::HeatMeter);
        di.addLinkMode(LinkMode::C1);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_TCH, 0x04, 0x28);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_NO_IV);

        addNumericFieldWithExtractor(
            "target",
            "Total energy consumption at the end of the year",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "target",
            "Date when previous year ended.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::PointInTime,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1)),
            Unit::DateLT
            );
    }
}

// Test: Howdy vario411 67627875 NOKEY
// telegram=|624468507578626728048C00F3900F002C25FEEB0600BA84134D9202A1327AFF003007102F2F_4406E1190000426CBF2C0F206730E2E7516874F5DB46B5A97816F575A29A1EA2717D6ADE5C2FE64517ED2B0497EE0FF64C2674CD0832572C484DDFED30|
// {"id": "67627875","media": "heat","meter": "vario411","name": "Howdy","target_date": "2021-12-31","target_kwh": 6625,"timestamp":"1111-11-11T11:11:11Z"}
// |Howdy;67627875;6625;2021-12-31;1111-11-11 11:11.11
