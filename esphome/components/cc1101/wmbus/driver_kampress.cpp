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
        di.setName("kampress");
        di.setDefaultFields("name,id,status,pressure_bar,max_pressure_bar,min_pressure_bar,timestamp");
        di.setMeterType(MeterType::PressureSensor);
        di.addLinkMode(LinkMode::C1);
        di.addDetection(MANUFACTURER_KAM,  0x18,  0x01);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addStringFieldWithExtractorAndLookup(
            "status",
            "Status and error flags.",
            DEFAULT_PRINT_PROPERTIES  | INCLUDE_TPL_STATUS,
            FieldMatcher::build()
            .set(VIFRange::ErrorFlags),
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::MapType::BitToString,
                        AlwaysTrigger, MaskBits(0xffff),
                        "OK",
                        {
                            { 0x01, "DROP" }, // Unexpected drop in pressure in relation to average pressure.
                            { 0x02, "SURGE" }, // Unexpected increase in pressure in relation to average pressure.
                            { 0x04, "HIGH" }, // Average pressure has reached configurable limit. Default 15 bar
                            { 0x08, "LOW" }, // Average pressure has reached configurable limit. Default 1.5 bar
                            { 0x10, "TRANSIENT" }, // Pressure changes quickly over short timeperiods. Average is fluctuating.
                            { 0x20, "COMM_ERROR" } // Cannot measure properly or bad internal communication.
                        }
                    },
                },
            });

        addNumericFieldWithExtractor(
            "pressure",
            "The measured pressure.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Pressure,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Pressure)
            );

        addNumericFieldWithExtractor(
            "max_pressure",
            "The maximum pressure measured during ?.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Pressure,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Maximum)
            .set(VIFRange::Pressure)
            );

        addNumericFieldWithExtractor(
            "min_pressure",
            "The minimum pressure measured during ?.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Pressure,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Minimum)
            .set(VIFRange::Pressure)
            );

        addNumericFieldWithExtractor(
            "alfa",
            "We do not know what this is.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("05FF09"))
            );

        addNumericFieldWithExtractor(
            "beta",
            "We do not know what this is.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("05FF0A"))
            );
    }
}

// Test: Pressing kampress 77000317 NOKEY
// telegram=|32442D2C1703007701188D280080E39322DB8F78_22696600126967000269660005FF091954A33A05FF0A99BD823A02FD170800|
// {"media":"pressure","meter":"kampress","name":"Pressing","id":"77000317","status":"LOW","pressure_bar":1.02,"max_pressure_bar":1.03,"min_pressure_bar":1.02,"alfa_counter":0.001246,"beta_counter":0.000997,"timestamp":"1111-11-11T11:11:11Z"}
// |Pressing;77000317;LOW;1.02;1.03;1.02;1111-11-11 11:11.11

// telegram=|27442D2C1703007701188D280194E393226EC679DE735657_660067006600962B913A21B9423A0800|
// {"media":"pressure","meter":"kampress","name":"Pressing","id":"77000317","status":"LOW","pressure_bar":1.02,"max_pressure_bar":1.03,"min_pressure_bar":1.02,"alfa_counter":0.001108,"beta_counter":0.000743,"timestamp":"1111-11-11T11:11:11Z"}
// |Pressing;77000317;LOW;1.02;1.03;1.02;1111-11-11 11:11.11

// telegram=|27442D2C1703007701188D289554F295224ED579DE73188A_650066006600E80EA43A6B97A3BA0800|
// {"media":"pressure","meter":"kampress","name":"Pressing","id":"77000317","status":"LOW","pressure_bar":1.02,"max_pressure_bar":1.02,"min_pressure_bar":1.01,"alfa_counter":0.001252,"beta_counter":-0.001248,"timestamp":"1111-11-11T11:11:11Z"}
// |Pressing;77000317;LOW;1.02;1.02;1.01;1111-11-11 11:11.11
