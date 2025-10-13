/*
 Copyright (C) 2018-2024 Fredrik Öhrström (gpl-3.0-or-later)
 Copyright (C)      2020 Eric Bus (gpl-3.0-or-later)
 Copyright (C)      2022 thecem (gpl-3.0-or-later)

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
        di.setName("kamheat");
        di.addNameAlias("multical302");
        di.addNameAlias("multical303");
        di.addNameAlias("multical403");
        di.addNameAlias("multical602");
        di.addNameAlias("multical603");
        di.addNameAlias("multical803");
        di.setDefaultFields("name,id,total_energy_consumption_kwh,total_volume_m3,status,timestamp");
        di.setMeterType(MeterType::HeatMeter);
        di.addLinkMode(LinkMode::C1);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_KAM, 0x04,  0x30); // 302
        di.addDetection(MANUFACTURER_KAM, 0x0d,  0x30); // 302
        di.addDetection(MANUFACTURER_KAM, 0x0c,  0x30); // 302
        di.addDetection(MANUFACTURER_KAM, 0x04,  0x40); // 303
        di.addDetection(MANUFACTURER_KAM, 0x0c,  0x40); // 303
        di.addDetection(MANUFACTURER_KAM, 0x04,  0x19); // 402
        di.addDetection(MANUFACTURER_KAM, 0x04,  0x34); // 403
        di.addDetection(MANUFACTURER_KAM, 0x0a,  0x34); // 403
        di.addDetection(MANUFACTURER_KAM, 0x0b,  0x34); // 403
        di.addDetection(MANUFACTURER_KAM, 0x0c,  0x34); // 403
        di.addDetection(MANUFACTURER_KAM, 0x0d,  0x34); // 403
        di.addDetection(MANUFACTURER_KAM, 0x04,  0x1c); // 602
        di.addDetection(MANUFACTURER_KAM, 0x04,  0x35); // 603
        di.addDetection(MANUFACTURER_KAM, 0x0c,  0x35); // 603
        di.addDetection(MANUFACTURER_KAM, 0x04,  0x39); // 803

        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addOptionalLibraryFields("fabrication_no,meter_datetime,on_time_h,on_time_at_error_h");
        addOptionalLibraryFields("flow_return_temperature_difference_c");

        // Technical Description Multical 603 page 116 section 7.7.2 Information code types on serial communication.
        addStringFieldWithExtractorAndLookup(
            "status",
            "Status and error flags.",
            DEFAULT_PRINT_PROPERTIES | PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS,
            FieldMatcher::build()
            .set(DifVifKey("04FF22")),
            Translate::Lookup(
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::MapType::BitToString,
                        AlwaysTrigger, MaskBits(0xffffffff),
                        "OK",
                        {
                            { 0x00000001, "VOLTAGE_INTERRUPTED" },
                            { 0x00000002, "LOW_BATTERY_LEVEL" },
                            { 0x00000004, "SENSOR_ERROR" },
                            { 0x00000008, "SENSOR_T1_ABOVE_MEASURING_RANGE" },
                            { 0x00000010, "SENSOR_T2_ABOVE_MEASURING_RANGE" },
                            { 0x00000020, "SENSOR_T1_BELOW_MEASURING_RANGE" },
                            { 0x00000040, "SENSOR_T2_BELOW_MEASURING_RANGE" },
                            { 0x00000080, "TEMP_DIFF_WRONG_POLARITY" },
                            { 0x00000100, "FLOW_SENSOR_WEAK_OR_AIR" },
                            { 0x00000200, "WRONG_FLOW_DIRECTION" },
                            { 0x00000400, "RESERVED_BIT_10" },
                            { 0x00000800, "FLOW_INCREASED" },
                            { 0x00001000, "IN_A1_LEAKAGE_IN_THE_SYSTEM" },
                            { 0x00002000, "IN_B1_LEAKAGE_IN_THE_SYSTEM" },
                            { 0x00004000, "IN-A1_A2_EXTERNAL_ALARM" },
                            { 0x00008000, "IN-B1_B2_EXTERNAL_ALARM" },
                            { 0x00010000, "V1_COMMUNICATION_ERROR" },
                            { 0x00020000, "V1_WRONG_PULSE_FIGURE" },
                            { 0x00040000, "IN_A2_LEAKAGE_IN_THE_SYSTEM" },
                            { 0x00080000, "IN_B2_LEAKAGE_IN_THE_SYSTEM" },
                            { 0x00100000, "T3_ABOVE_MEASURING_RANGE_OR_SWITCHED_OFF" },
                            { 0x00200000, "T3_BELOW_MEASURING_RANGE_OR_SHORT_CIRCUITED" },
                            { 0x00400000, "V2_COMMUNICATION_ERROR" },
                            { 0x00800000, "V2_WRONG_PULSE_FIGURE" },
                            { 0x01000000, "V2_AIR" },
                            { 0x02000000, "V2_WRONG_FLOW_DIRECTION" },
                            { 0x04000000, "RESERVED_BIT_26" },
                            { 0x08000000, "V2_INCREASED_FLOW" },
                            { 0x10000000, "V1_V2_BURST_WATER_LOSS" },
                            { 0x20000000, "V1_V2_BURST_WATER_PENETRATION" },
                            { 0x40000000, "V1_V2_LEAKAGE_WATER_LOSS" },
                            { 0x80000000, "V1_V2_LEAKAGE_WATER_PENETRATION" }
                        }
                    },
                },
            }));

        addNumericFieldWithExtractor(
            "total_energy_consumption",
            "The total energy consumption recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            );

        addNumericFieldWithExtractor(
            "total_volume",
            "The volume of water (3/68/Volume V1).",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            );

        addNumericFieldWithExtractor(
            "volume_flow",
            "The actual amount of water that pass through this meter (8/74/Flow V1 actual).",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Flow,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::VolumeFlow)
            );

        addNumericFieldWithExtractor(
            "power",
            "The current power flowing.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            );

        addNumericFieldWithExtractor(
            "max_power",
            "The maximum power supplied.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Maximum)
            .set(VIFRange::AnyPowerVIF)
            );

        addNumericFieldWithExtractor(
            "t1_temperature",
            "The forward temperature of the water (6/86/t2 actual 2 decimals).",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::FlowTemperature)
            );

        addNumericFieldWithExtractor(
            "t2_temperature",
            "The return temperature of the water (7/87/t2 actual 2 decimals).",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ReturnTemperature)
            );

        addNumericFieldWithExtractor(
            "max_flow",
            "The maximum flow of water that passed through this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Flow,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Maximum)
            .set(VIFRange::VolumeFlow)
            );

        addNumericFieldWithExtractor(
            "forward_energy",
            "The forward energy of the water (4/97/Energy E8).",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("04FF07")),
            Unit::M3C);

        addNumericFieldWithExtractor(
            "return_energy",
            "The return energy of the water (5/110/Energy E9).",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("04FF08")),
            Unit::M3C);

        addStringFieldWithExtractor(
            "meter_date",
            "The date and time (10/348/Date and time).",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            );

        addNumericFieldWithExtractor(
            "target_energy",
            "The energy consumption recorded by this meter at the set date (11/60/Heat energy E1/026C).",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "target_volume",
            "The amount of water that had passed through this meter at the set date (13/68/Volume V1).",
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
            "The most recent billing period date and time (14/348/Date and Time logged).",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "operating_time",
            "How long the meter has been collecting data.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Time,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::OperatingTime)
            .add(VIFCombinable::Mfct21)
            );

    }
}

// Test: MyHeater multical302 67676767 NOKEY
// Comment: Using vif kwh
// telegram=|2E442D2C6767676730048D2039D1684020_BCDB7803062C000043060000000314630000426C7F2A022D130001FF2100|
// {"id": "67676767","media": "heat","meter": "kamheat","name": "MyHeater","power_kw": 1.9,"status": "OK","target_date": "2019-10-31","target_energy_kwh": 0,"timestamp": "1111-11-11T11:11:11Z","total_energy_consumption_kwh": 44,"total_volume_m3": 0.99}
// |MyHeater;67676767;44;0.99;OK;1111-11-11 11:11.11

// telegram=|25442D2C6767676730048D203AD2684020_D81579E7F1D5902C00000000006300007F2A130000|
// {"id": "67676767","media": "heat","meter": "kamheat","name": "MyHeater","power_kw": 1.9,"status": "OK","target_date": "2019-10-31","target_energy_kwh": 0,"timestamp": "1111-11-11T11:11:11Z","total_energy_consumption_kwh": 44,"total_volume_m3": 0.99}
// |MyHeater;67676767;44;0.99;OK;1111-11-11 11:11.11

// Test: MyHeaterMj multical302 46464646 NOKEY
// Comment: Using mj kwh
// telegram=|2E442D2C46464646300C8D207A70EA6021B1C178_030FC51000430F9210000314072B05426CBE2B022D0C0001FF2100|
// {"id": "46464646","media": "heat volume at inlet","meter": "kamheat","name": "MyHeaterMj","power_kw": 1.2,"status": "OK","target_date": "2021-11-30","target_energy_kwh": 11783.333333,"timestamp": "1111-11-11T11:11:11Z","total_energy_consumption_kwh": 11925,"total_volume_m3": 3386.95}
// |MyHeaterMj;46464646;11925;3386.95;OK;1111-11-11 11:11.11

// telegram=|25442D2C46464646300C8D20D3E2EB60212B6D79E26DCD65_C51000921000152B05BE2B0C0000|
// {"id": "46464646","media": "heat volume at inlet","meter": "kamheat","name": "MyHeaterMj","power_kw": 1.2,"status": "OK","target_date": "2021-11-30","target_energy_kwh": 11783.333333,"timestamp": "1111-11-11T11:11:11Z","total_energy_consumption_kwh": 11925,"total_volume_m3": 3387.09}
// |MyHeaterMj;46464646;11925;3387.09;OK;1111-11-11 11:11.11

// Test: Heat multical603 36363636 NOKEY
// Comment:
// telegram=|42442D2C3636363635048D20E18025B62087D078_0406A500000004FF072B01000004FF089C000000041421020000043B120000000259D014025D000904FF2200000000|
// {"media":"heat","meter":"kamheat","name":"Heat","id":"36363636","status":"OK","total_energy_consumption_kwh":165,"total_volume_m3":5.45,"volume_flow_m3h":0.018,"t1_temperature_c":53.28,"t2_temperature_c":23.04,"forward_energy_m3c":299,"return_energy_m3c":156,"timestamp":"1111-11-11T11:11:11Z"}
// |Heat;36363636;165;5.45;OK;1111-11-11 11:11.11

// Test: HeatInlet multical603 66666666 NOKEY
// telegram=|5A442D2C66666666350C8D2066D0E16420C6A178_0406051C000004FF07393D000004FF08AE2400000414F7680000043B47000000042D1600000002596D14025DFD0804FF22000000000422E61A0000143B8C010000142D7C000000|
// {"media":"heat volume at inlet","meter":"kamheat","name":"HeatInlet","id":"66666666","on_time_h":6886,"status":"OK","total_energy_consumption_kwh":7173,"total_volume_m3":268.71,"volume_flow_m3h":0.071,"power_kw":2.2,"max_power_kw":12.4,"t1_temperature_c":52.29,"t2_temperature_c":23.01,"max_flow_m3h":0.396,"forward_energy_m3c":15673,"return_energy_m3c":9390,"timestamp":"1111-11-11T11:11:11Z"}
// |HeatInlet;66666666;7173;268.71;OK;1111-11-11 11:11.11

// Test: My403Cooling multical403 78780102 NOKEY
// telegram=|88442D2C02017878340A8D208D529C132037FC78_040E2D0A000004FF07F8FF000004FF08401801000413C1900500844014000000008480401400000000043BED0000000259BC06025DCD07142DE7FFFFFF84100E0000000084200E0000000004FF2200000000026C9228440E5F0300004413960D0200C4401400000000C480401400000000426C8128|
// {"forward_energy_m3c": 65528,"id": "78780102","max_power_kw": -2.5,"media": "cooling load volume at outlet","meter": "kamheat","meter_date": "2020-08-18","name": "My403Cooling","return_energy_m3c": 71744,"status": "OK","t1_temperature_c": 17.24,"t2_temperature_c": 19.97,"target_date": "2020-08-01","target_energy_kwh": 239.722222,"target_volume_m3": 134.55,"timestamp": "1111-11-11T11:11:11Z","total_energy_consumption_kwh": 723.611111,"total_volume_m3": 364.737,"volume_flow_m3h": 0.237}
// |My403Cooling;78780102;723.611111;364.737;OK;1111-11-11 11:11.11

// telegram=|5B442D2C02017878340A8D2096809C1320EF2B7934147ED7_2D0A0000FAFF000043180100CE9005000000000000000000EE000000BA06CB07E7FFFFFF00000000000000000000000092285F030000960D020000000000000000008128|
// {"forward_energy_m3c": 65530,"id": "78780102","max_power_kw": -2.5,"media": "cooling load volume at outlet","meter": "kamheat","meter_date": "2020-08-18","name": "My403Cooling","return_energy_m3c": 71747,"status": "OK","t1_temperature_c": 17.22,"t2_temperature_c": 19.95,"target_date": "2020-08-01","target_energy_kwh": 239.722222,"target_volume_m3": 134.55,"timestamp": "1111-11-11T11:11:11Z","total_energy_consumption_kwh": 723.611111,"total_volume_m3": 364.75,"volume_flow_m3h": 0.238}
// |My403Cooling;78780102;723.611111;364.75;OK;1111-11-11 11:11.11

// Test: Heato multical602 78152801 NOKEY
// telegram=|7A442D2C012815781C048D206450E76322344678_02F9FF1511130406690B010004EEFF07C1BC020004EEFF0890D401000414A925040084401400000000848040140000000002FD170000026CB929426CBF284406100A01004414D81A0400C4401400000000C480401400000000043B3900000002592A17025D2912|
// {"id": "78152801","media": "heat","meter": "kamheat","meter_date": "2021-09-25","name": "Heato","status": "OK","t1_temperature_c": 59.3,"t2_temperature_c": 46.49,"target_date": "2021-08-31","target_energy_kwh": 68112,"target_volume_m3": 2690.16,"timestamp": "1111-11-11T11:11:11Z","total_energy_consumption_kwh": 68457,"total_volume_m3": 2717.85,"volume_flow_m3h": 0.057}
// |Heato;78152801;68457;2717.85;OK;1111-11-11 11:11.11

// telegram=|4F442D2C012815781C048D206551E76322BE767900843005_1113690B0100C1BC020090D40100A925040000000000000000000000B929BF28100A0100D81A04000000000000000000390000002A172912|
// {"id": "78152801","media": "heat","meter": "kamheat","meter_date": "2021-09-25","name": "Heato","status": "OK","t1_temperature_c": 59.3,"t2_temperature_c": 46.49,"target_date": "2021-08-31","target_energy_kwh": 68112,"target_volume_m3": 2690.16,"timestamp": "1111-11-11T11:11:11Z","total_energy_consumption_kwh": 68457,"total_volume_m3": 2717.85,"volume_flow_m3h": 0.057}
// |Heato;78152801;68457;2717.85;OK;1111-11-11 11:11.11

// Test: Heater multical803 80808081 NOKEY
// telegram=|88442D2C8180808039048D208640513220EA7978_040FA000000004FF070200000004FF08090000000414FF000000844014000000008480401400000000043B0000000002590000025D0000142D0000000084100F0000000084200F0000000004FF2260000100026C892B440F00000000441400000000C4401400000000C480401400000000426C812B|
// {"forward_energy_m3c": 2,"id": "80808081","max_power_kw": 0,"media": "heat","meter": "kamheat","meter_date": "2020-11-09","name": "Heater","return_energy_m3c": 9,"status": "SENSOR_T1_BELOW_MEASURING_RANGE SENSOR_T2_BELOW_MEASURING_RANGE V1_COMMUNICATION_ERROR","t1_temperature_c": 0,"t2_temperature_c": 0,"target_date": "2020-11-01","target_energy_kwh": 0,"target_volume_m3": 0,"timestamp": "1111-11-11T11:11:11Z","total_energy_consumption_kwh": 444.444444,"total_volume_m3": 2.55,"volume_flow_m3h": 0}
// |Heater;80808081;444.444444;2.55;SENSOR_T1_BELOW_MEASURING_RANGE SENSOR_T2_BELOW_MEASURING_RANGE V1_COMMUNICATION_ERROR;1111-11-11 11:11.11

// Test: Kamstrup_303 kamheat 78787878 NOKEY
// telegram=|5E442D2C78787878400C7A6E0050252F2F_04056C2B000004138A0B070004FF07C657020004FF08FD36020002594B09025DFA08023B000002FF220000026CF42144052F000000441302AD0000426CE1212F2F2F2F2F2F2F2F2F2F2F2F2F2F2F|
// {"media":"heat volume at inlet","meter":"kamheat","name":"Kamstrup_303","id":"78787878","status":"OK","total_energy_consumption_kwh":1111.6,"total_volume_m3":461.706,"volume_flow_m3h":0,"t1_temperature_c":23.79,"t2_temperature_c":22.98,"forward_energy_m3c":153542,"return_energy_m3c":145149,"meter_date":"2023-01-20","target_energy_kwh":4.7,"target_volume_m3":44.29,"target_date":"2023-01-01","timestamp":"1111-11-11T11:11:11Z"}
// |Kamstrup_303;78787878;1111.6;461.706;OK;1111-11-11 11:11.11

// Test: Kamstrup_403_mbus kamheat 77447744 NOKEY
// telegram=|68464668084a72447744772d2c3404060000000406ce86000004ff073444020004ff08f8ce0100041411680300043B0f02000002593c19025da41104ff220000000004a5ff21c7d02700d916|
// {"forward_energy_m3c": 148532,"id": "77447744","media": "heat","meter": "kamheat","name": "Kamstrup_403_mbus","return_energy_m3c": 118520,"status": "OK","t1_temperature_c": 64.6,"t2_temperature_c": 45.16,"timestamp": "1111-11-11T11:11:11Z","total_energy_consumption_kwh": 34510,"total_volume_m3": 2232.49,"volume_flow_m3h": 0.527,"operating_time_h": 43489.183333}
// |Kamstrup_403_mbus;77447744;34510;2232.49;OK;1111-11-11 11:11.11

// Test: Kamstrup_402_wmbus kamheat 62215006 NOKEY
// telegram=|40442D2C0650216219048D2083A4E1162306FF78_040F2C3F000004FF07DBA40D0004FF08860B0D000414BA33140002FD170000043B620000000259A21E025DFA1B|
// {"media":"heat","meter":"kamheat","name":"Kamstrup_402_wmbus","id":"62215006","forward_energy_m3c":894171,"return_energy_m3c":854918,"t1_temperature_c":78.42,"t2_temperature_c":71.62,"total_energy_consumption_kwh":44922.222222,"total_volume_m3":13239.62,"volume_flow_m3h":0.098,"status":"OK","timestamp":"1111-11-11T11:11:11Z"}
// |Kamstrup_402_wmbus;62215006;44922.222222;13239.62;OK;1111-11-11 11:11.11

// Test: Kamstrup_MC603_mbus kamheat 32323232 NOKEY
// telegram=|68c9c96808e672323232322d2c35041900000004fB006083000004ff074006010004ff08299400000416984e010084401400000000848040140000000004225043000034221c0000000259c91f025d4f1102617a0e042e30020000142e65030000043c24050000143ce308000004ff2200000000046d2e2B0f3144fB00007d000044ff07Bdf9000044ff08308d00004416B73f0100c4401400000000c480401400000000542ed9020000543ce8090000426c013102ff1a011B0c783032858404ff16e5841e0004ff17c1d5B400a516|
// {"fabrication_no": "84853230",  "flow_return_temperature_difference_c": 37.06,  "forward_energy_m3c": 67136,  "id": "32323232",  "max_flow_m3h": 22.75,  "max_power_kw": 869,  "media": "heat",  "meter": "kamheat",  "meter_datetime": "2024-01-15 11:46",  "name": "Kamstrup_MC603_mbus",  "on_time_at_error_h": 28,  "on_time_h": 17232,  "power_kw": 560,  "return_energy_m3c": 37929,  "status": "OK",  "t1_temperature_c": 81.37,  "t2_temperature_c": 44.31,  "target_date": "2024-01-01",  "target_energy_kwh": 3200000,  "target_volume_m3": 81847,  "timestamp": "1111-11-11T11:11:11Z",  "total_energy_consumption_kwh": 3363200,  "total_volume_m3": 85656,  "volume_flow_m3h": 13.16}
// |Kamstrup_MC603_mbus;32323232;3363200;85656;OK;1111-11-11 11:11.11

// Test: KMHEAT kamheat 85412440 NOKEY
// telegram=|5e442d2c4024418535047ae10050252f2f04fB091300000004167500000004ff2200000000043ca301000002599c1d025dB00e844014000000008480401400000000042eB9000000026c0534426c013444fB0900000000543c000000002f2f|
// {"id": "85412440","media": "heat","meter": "kamheat","meter_date": "2024-04-05","name": "KMHEAT","power_kw": 185,"status": "OK","t1_temperature_c": 75.8,"t2_temperature_c": 37.6,"target_date": "2024-04-01","target_energy_kwh": 0,"timestamp": "1111-11-11T11:11:11Z","total_energy_consumption_kwh": 5277.777778,"total_volume_m3": 117,"volume_flow_m3h": 4.19}
// |KMHEAT;85412440;5277.777778;117;OK;1111-11-11 11:11.11

// Test: KM2 kamheat 85007420 NOKEY
// telegram=|689f9f68081472207400852d2c350cad00000004fb08c304000004ff07f21c000004ff084d1100000415fe29000084401400000000848040140000000004228829000034220a00000002598511025dab100261da00042d00000000142d08000000043b00000000143b6000000004ff2200000000046d0d2c0238542dc9010000543be1050000426c013102ff1a011b0c782074008504ff16850b200004ff17c9ff0e01cb16|
// {"fabrication_no": "85007420","flow_return_temperature_difference_c": 2.18,"forward_energy_m3c": 7410,"id": "85007420","max_flow_m3h": 0.096,"max_power_kw": 0.8,"media": "heat volume at inlet","meter": "kamheat","meter_datetime": "2024-08-02 12:13","name": "KM2","on_time_at_error_h": 10,"on_time_h": 10632,"power_kw": 0,"return_energy_m3c": 4429,"status": "OK","t1_temperature_c": 44.85,"t2_temperature_c": 42.67,"target_date": "2024-01-01","timestamp": "1111-11-11T11:11:11Z","total_energy_consumption_kwh": 33861.111111,"total_volume_m3": 1075,"volume_flow_m3h": 0}
// |KM2;85007420;33861.111111;1075;OK;1111-11-11 11:11.11
