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
        di.setName("ultraheat");
        di.setDefaultFields("name,id,heat_kwh,timestamp");
        di.setMeterType(MeterType::HeatMeter);
        di.addDetection(MANUFACTURER_LUG, 0x04,  0x04);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addOptionalLibraryFields("meter_datetime,fabrication_no");

        addNumericFieldWithExtractor(
            "heat",
            "The total heat energy consumption recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            );

        addNumericFieldWithExtractor(
            "volume",
            "The total heating media volume recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            );

        addNumericFieldWithExtractor(
            "power",
            "The current power consumption.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::PowerW)
            );

        addNumericFieldWithExtractor(
            "flow",
            "The current heat media volume flow.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Flow,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::VolumeFlow)
            );

        addNumericFieldWithExtractor(
            "flow",
            "The current forward heat media temperature.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::FlowTemperature)
            );

        addNumericFieldWithExtractor(
            "return",
            "The current return heat media temperature.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ReturnTemperature)
            );

        addStringFieldWithExtractorAndLookup(
            "status",
            "Meter status.",
            PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS,
            FieldMatcher::build()
            .set(VIFRange::ErrorFlags),
            Translate::Lookup(
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::MapType::BitToString,
                        AlwaysTrigger, MaskBits(0xffff),
                        "OK",
                        {
                            /* TODO */
                        }
                    },
                },
            }));

    }
}

// Test: MyUltra ultraheat 70444600 NOKEY
// telegram=|68F8F86808007200464470A7320404270000000974040970040C0E082303000C14079519000B2D0500000B3B0808000A5B52000A5F51000A6206004C14061818004C0E490603000C7800464470891071609B102D020100DB102D0201009B103B6009009A105B78009A105F74000C22726701003C22000000007C2200000000426C01018C2006000000008C3006000000008C80100600000000CC200600000000CC300600000000CC801006000000009A115B64009A115F63009B113B5208009B112D020100BC0122000000008C010E490603008C2106000000008C3106000000008C811006000000008C011406181800046D310ACA210F21040010A0C116|
// {"media":"heat","status":"OK","meter":"ultraheat","meter_datetime": "2022-01-10 10:49", "name":"MyUltra","id":"70444600","heat_kwh":8974.444444,"volume_m3":1995.07,"power_kw":0.5,"flow_m3h":0.808,"flow_c":52,"return_c":51,"fabrication_no": "70444600","timestamp":"1111-11-11T11:11:11Z"}
// |MyUltra;70444600;8974.444444;1111-11-11 11:11.11

// Test: MyUltra2 ultraheat 71635605 NOKEY
// telegram=|3b44a7320556637104047afa2000202f2f0c06774202000c14399956000b2d0200f00b3b3018000a5a51030a5e520302fd170000066d0c080af42500|
// {"flow_c": 35.1,"flow_m3h": 1.83,"heat_kwh": 24277,"id": "71635605","media": "heat","meter": "ultraheat","meter_datetime": "2023-05-20 10:08:12","name": "MyUltra2","power_kw": -0.2,"return_c": 35.2,"status": "UNKNOWN_20","timestamp": "1111-11-11T11:11:11Z","volume_m3": 5699.39}
// |MyUltra2;71635605;24277;1111-11-11 11:11.11
