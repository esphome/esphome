/*
 Copyright (C) 2018-2022 Fredrik Öhrström (gpl-3.0-or-later)

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
        di.setName("hydrocalm3");
        di.setDefaultFields("name,id,total_heating_kwh,total_cooling_kwh,timestamp");
        di.setMeterType(MeterType::HeatMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_BMT, 0x0d,  0x0b);
        di.addDetection(MANUFACTURER_BMT, 0x0d,  0x1a); // HYDROCAL-M4
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) :  MeterCommonImplementation(mi, di)
    {
        setMfctTPLStatusBits(
            Translate::Lookup()
            .add(Translate::Rule("TPL_STS", Translate::MapType::BitToString)
                 .set(MaskBits(0xe0))
                 .set(DefaultMessage("OK"))
                 .add(Translate::Map(0x80 ,"SABOTAGE_ENCLOSURE", TestBit::Set))));

        addStringField(
            "status",
            "Meter status from tpl status field.",
            DEFAULT_PRINT_PROPERTIES  |
            PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS);

        addNumericFieldWithExtractor(
            "total_heating",
            "The total heating energy consumption recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(IndexNr(1))
            );

        addNumericFieldWithExtractor(
            "device",
            "The date time when the recording was made.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::PointInTime,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            );

        addNumericFieldWithExtractor(
            "total_cooling",
            "The total cooling energy consumption recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(IndexNr(2))
            );

        addNumericFieldWithExtractor(
            "total_heating",
            "Total heating volume of media.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(IndexNr(1))
            );

        addNumericFieldWithExtractor(
            "total_cooling",
            "Total cooling volume of media.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(IndexNr(2))
            );

        addNumericFieldWithExtractor(
            "c1_volume",
            "Supply c1 volume.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(IndexNr(3))
            );

        addNumericFieldWithExtractor(
            "c2_volume",
            "Return c2 volume.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(IndexNr(4))
            );

        addNumericFieldWithExtractor(
            "supply_temperature",
            "The supply t1 pipe temperature.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::FlowTemperature)
            .set(IndexNr(1))
            );

        addNumericFieldWithExtractor(
            "return_temperature",
            "The return t2 pipe temperature.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ReturnTemperature)
            );

     }
}

// Test: HeatCool hydrocalm3 71727374 NOKEY
// telegram=|8E44B409747372710B0D7A798080052F2F_0C0E59600100046D1D36B9290C13679947000C0E000000000C13590000000C13000000000C13000000000A5A18020A5E11020F823D06003D06003D06003D0600140600620500480400E402001601000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002F2F|
// {"media":"heat/cooling load","meter":"hydrocalm3","name":"HeatCool","id":"71727374","status": "SABOTAGE_ENCLOSURE","total_heating_kwh":4460.833333,"total_cooling_kwh":0,"device_datetime":"2021-09-25 22:29","total_heating_m3":479.967,"total_cooling_m3":0.059,"c1_volume_m3":0,"c2_volume_m3":0,"supply_temperature_c":21.8,"return_temperature_c":21.1,"timestamp":"1111-11-11T11:11:11Z"}
// |HeatCool;71727374;4460.833333;0;1111-11-11 11:11.11
//
// Test: testm4 hydrocalm3 38031627 NOKEY
// telegram=|2C44B409271603381A0D8C208A7ACE000020_046D0CB6ED240C03267300000C13130800000F6300000000000000|
// {"media":"heat/cooling load","meter":"hydrocalm3","name":"testm4","id":"38031627","status":"OK","total_heating_kwh":7.326,"device_datetime":"2023-04-13 22:12","total_heating_m3":0.813,"timestamp":"1111-11-11T11:11:11Z"}
// |testm4;38031627;7.326;null;1111-11-11 11:11.11
