/*
 Copyright (C) 2019 Jacek Tomasiak (gpl-3.0-or-later)
 Copyright (C) 2020-2023 Fredrik Öhrström (gpl-3.0-or-later)
 Copyright (C) 2021 Vincent Privat (gpl-3.0-or-later)

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
#include"manufacturer_specificities.h"

namespace
{
    /** Contains all the booleans required to store the alarms of a PRIOS device. */
    struct IzarAlarms
    {
        bool general_alarm;
        bool leakage_currently;
        bool leakage_previously;
        bool meter_blocked;
        bool back_flow;
        bool underflow;
        bool overflow;
        bool submarine;
        bool sensor_fraud_currently;
        bool sensor_fraud_previously;
        bool mechanical_fraud_currently;
        bool mechanical_fraud_previously;
    };

    struct Driver : public virtual MeterCommonImplementation
    {
        Driver(MeterInfo &mi, DriverInfo &di);

        void processContent(Telegram *t);

    private:

        string currentAlarmsText(IzarAlarms &alarms);
        string previousAlarmsText(IzarAlarms &alarms);

        vector<uchar> decodePrios(const vector<uchar> &origin, const vector<uchar> &payload, uint32_t key);

        vector<uint32_t> keys;
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("izar");
        di.setDefaultFields("name,id,prefix,serial_number,total_m3,last_month_total_m3,"
                            "last_month_measure_date,"
                            "remaining_battery_life_y,"
                            "current_alarms,"
                            "previous_alarms,"
                            "transmit_period_s,"
                            "manufacture_year,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_HYD,  0x07,  0x85);
        di.addDetection(MANUFACTURER_SAP,  0x15,    -1);
        di.addDetection(MANUFACTURER_SAP,  0x04,    -1);
        di.addDetection(MANUFACTURER_SAP,  0x07,  0x00);
        di.addDetection(MANUFACTURER_DME,  0x07,  0x78);
        di.addDetection(MANUFACTURER_DME,  0x06,  0x78);
        di.addDetection(MANUFACTURER_HYD,  0x07,  0x86);
        di.usesProcessContent();

        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        initializeDiehlDefaultKeySupport(meterKeys()->confidentiality_key, keys);

        addStringField("prefix",
                       "The alphanumeric prefix printed before serial number on device.",
                       DEFAULT_PRINT_PROPERTIES);

        addStringField("serial_number",
                       "The meter serial number.",
                       DEFAULT_PRINT_PROPERTIES);

        addNumericField("total",
                        Quantity::Volume,
                        DEFAULT_PRINT_PROPERTIES,
                        "The total water consumption recorded by this meter.");

        addNumericField("last_month_total",
                        Quantity::Volume,
                        DEFAULT_PRINT_PROPERTIES,
                        "The total water consumption recorded by this meter around end of last month.");

        addStringField("last_month_measure_date",
                       "The date when the meter recorded the most recent billing value.",
                       DEFAULT_PRINT_PROPERTIES);

        addNumericField("remaining_battery_life",
                        Quantity::Time,
                        DEFAULT_PRINT_PROPERTIES,
                        "How many more years the battery is expected to last",
                        Unit::Year);
        addStringField("current_alarms",
                       "Alarms currently reported by the meter.",
                       DEFAULT_PRINT_PROPERTIES);

        addStringField("previous_alarms",
                       "Alarms previously reported by the meter.",
                       DEFAULT_PRINT_PROPERTIES);

        addNumericField("transmit_period", Quantity::Time,
                        DEFAULT_PRINT_PROPERTIES,
                        "The period at which the meter transmits its data.",
                        Unit::Second);

        addStringField("manufacture_year",
                       "The year during which the meter was manufactured.",
                       DEFAULT_PRINT_PROPERTIES);
    }

    string Driver::currentAlarmsText(IzarAlarms &alarms)
    {
        string s;
        if (alarms.leakage_currently) {
            s.append("leakage,");
        }
        if (alarms.meter_blocked) {
            s.append("meter_blocked,");
        }
        if (alarms.back_flow) {
            s.append("back_flow,");
        }
        if (alarms.underflow) {
            s.append("underflow,");
        }
        if (alarms.overflow) {
            s.append("overflow,");
        }
        if (alarms.submarine) {
            s.append("submarine,");
        }
        if (alarms.sensor_fraud_currently) {
            s.append("sensor_fraud,");
        }
        if (alarms.mechanical_fraud_currently) {
            s.append("mechanical_fraud,");
        }
        if (s.length() > 0) {
            if (alarms.general_alarm) {
                return "general_alarm";
            }
            s.pop_back();
            return s;
        }
        return "no_alarm";
    }

    string Driver::previousAlarmsText(IzarAlarms &alarms)
    {
        string s;
        if (alarms.leakage_previously) {
            s.append("leakage,");
        }
        if (alarms.sensor_fraud_previously) {
            s.append("sensor_fraud,");
        }
        if (alarms.mechanical_fraud_previously) {
            s.append("mechanical_fraud,");
        }
        if (s.length() > 0) {
            s.pop_back();
            return s;
        }
        return "no_alarm";
    }

    void Driver::processContent(Telegram *t)
    {
        vector<uchar> frame;
        t->extractFrame(&frame);
        vector<uchar> origin = t->original.empty() ? frame : t->original;

        vector<uchar> decoded_content;
        for (auto& key : keys) {
            decoded_content = decodePrios(origin, frame, key);
            if (!decoded_content.empty())
                break;
        }

        debug("(izar) Decoded PRIOS data: %s", bin2hex(decoded_content).c_str());

        if (decoded_content.empty())
        {
            if (t->beingAnalyzed() == false)
            {
                warning("(izar) Decoding PRIOS data failed. Ignoring telegram.");
            }
            return;
        }

        if (detectDiehlFrameInterpretation(frame) == DiehlFrameInterpretation::SAP_PRIOS)
        {
            string digits = to_string((origin[7] & 0x03) << 24 | origin[6] << 16 | origin[5] << 8 | origin[4]);
            digits = tostrprintf("%08d", atoi(digits.c_str())); // Make sure we are on 8 digits for 200x years
            // get the manufacture year
            uint8_t yy = atoi(digits.substr(0, 2).c_str());
            int manufacture_year = yy > 70 ? (1900 + yy) : (2000 + yy); // Maybe to adjust in 2070, if this code stills lives :D
            setStringValue("manufacture_year", tostrprintf("%d", manufacture_year), NULL);

            // get the serial number
            uint32_t serial_number = atoi(digits.substr(2, digits.size()).c_str());
            setStringValue("serial_number", tostrprintf("%06d", serial_number), NULL);

            // get letters
            uchar supplier_code = '@' + (((origin[9] & 0x0F) << 1) | (origin[8] >> 7));
            uchar meter_type = '@' + ((origin[8] & 0x7C) >> 2);
            uchar diameter = '@' + (((origin[8] & 0x03) << 3) | (origin[7] >> 5));
            // build the prefix
            string prefix = tostrprintf("%c%02d%c%c", supplier_code, yy, meter_type, diameter);
            setStringValue("prefix", prefix, NULL);
        }

        // get the remaining battery life (in year) and transmission period (in seconds)
        double remaining_battery_life = (frame[12] & 0x1F) / 2.0;
        setNumericValue("remaining_battery_life", Unit::Year, remaining_battery_life);

        int transmit_period_s = 1 << ((frame[11] & 0x0F) + 2);
        setNumericValue("transmit_period", Unit::Second, transmit_period_s);

        double total_water_consumption_l_ = uint32FromBytes(decoded_content, 1, true);
        setNumericValue("total", Unit::L, total_water_consumption_l_);

        if (decoded_content.size() > 8) {
            double last_month_total_water_consumption_l_ = uint32FromBytes(decoded_content, 5, true);
            setNumericValue("last_month_total", Unit::L, last_month_total_water_consumption_l_);
        }
        
        // get the date when the second measurement was taken
        if (decoded_content.size() > 10) {
            uint16_t h0_year = ((decoded_content[10] & 0xF0) >> 1) + ((decoded_content[9] & 0xE0) >> 5);
            if (h0_year > 80) {
                h0_year += 1900;
            } else {
                h0_year += 2000;
            }
            uint8_t h0_month = decoded_content[10] & 0xF;
            uint8_t h0_day = decoded_content[9] & 0x1F;

            setStringValue("last_month_measure_date", tostrprintf("%d-%02d-%02d", h0_year, h0_month%99, h0_day%99), NULL);
        }

        // read the alarms:
        IzarAlarms alarms {};

        alarms.general_alarm = frame[11] >> 7;
        alarms.leakage_currently = frame[12] >> 7;
        alarms.leakage_previously = frame[12] >> 6 & 0x1;
        alarms.meter_blocked = frame[12] >> 5 & 0x1;
        alarms.back_flow = frame[13] >> 7;
        alarms.underflow = frame[13] >> 6 & 0x1;
        alarms.overflow = frame[13] >> 5 & 0x1;
        alarms.submarine = frame[13] >> 4 & 0x1;
        alarms.sensor_fraud_currently = frame[13] >> 3 & 0x1;
        alarms.sensor_fraud_previously = frame[13] >> 2 & 0x1;
        alarms.mechanical_fraud_currently = frame[13] >> 1 & 0x1;
        alarms.mechanical_fraud_previously = frame[13] & 0x1;

        setStringValue("current_alarms", currentAlarmsText(alarms));
        setStringValue("previous_alarms", previousAlarmsText(alarms));
    }

    vector<uchar> Driver::decodePrios(const vector<uchar> &origin, const vector<uchar> &frame, uint32_t key)
    {
        return decodeDiehlLfsr(origin, frame, key, DiehlLfsrCheckMethod::HEADER_1_BYTE, 0x4B);
    }
}

// Test: IzarWater izar 21242472 NOKEY
// telegram=|1944304C72242421D401A2_013D4013DD8B46A4999C1293E582CC|
// {"media":"water","meter":"izar","name":"IzarWater","id":"21242472","prefix":"C19UA","serial_number":"145842","total_m3":3.488,"last_month_total_m3":3.486,"last_month_measure_date":"2019-09-30","remaining_battery_life_y":14.5,"current_alarms":"meter_blocked,underflow","previous_alarms":"no_alarm","transmit_period_s":8,"manufacture_year":"2019","timestamp":"1111-11-11T11:11:11Z"}
// |IzarWater;21242472;C19UA;145842;3.488;3.486;2019-09-30;14.5;meter_blocked,underflow;no_alarm;8;2019;1111-11-11 11:11.11

// Test: IzarWater2 izar 66236629 NOKEY
// telegram=|2944A511780729662366A20118001378D3B3DB8CEDD77731F25832AAF3DA8CADF9774EA673172E8C61F2|
// {"media":"water","meter":"izar","name":"IzarWater2","id":"66236629","total_m3":16.76,"last_month_total_m3":11.84,"last_month_measure_date":"2019-11-30","remaining_battery_life_y":12,"current_alarms":"no_alarm","previous_alarms":"no_alarm","transmit_period_s":8,"timestamp":"1111-11-11T11:11:11Z"}
// |IzarWater2;66236629;null;null;16.76;11.84;2019-11-30;12;no_alarm;no_alarm;8;null;1111-11-11 11:11.11

// Test: IzarWater3 izar 20481979 NOKEY
// telegram=|1944A511780779194820A1_21170013355F8EDB2D03C6912B1E37
// {"media":"water","meter":"izar","name":"IzarWater3","id":"20481979","total_m3":4.366,"last_month_total_m3":0,"last_month_measure_date":"2020-12-31","remaining_battery_life_y":11.5,"current_alarms":"no_alarm","previous_alarms":"no_alarm","transmit_period_s":8,"timestamp":"1111-11-11T11:11:11Z"}
// |IzarWater3;20481979;null;null;4.366;0;2020-12-31;11.5;no_alarm;no_alarm;8;null;1111-11-11 11:11.11

// Test: IzarWater4 izar 2124589c NOKEY
// Comment: With mfct specific tpl ci field a3.
// telegram=|1944304c9c5824210c04a363140013716577ec59e8663ab0d31c|
// {"media":"water","meter":"izar","name":"IzarWater4","id":"2124589c","prefix":"H19CA","serial_number":"159196","total_m3":38.944,"last_month_total_m3":38.691,"last_month_measure_date":"2021-02-01","remaining_battery_life_y":10,"current_alarms":"no_alarm","previous_alarms":"no_alarm","transmit_period_s":32,"manufacture_year":"2019","timestamp":"1111-11-11T11:11:11Z"}
// |IzarWater4;2124589c;H19CA;159196;38.944;38.691;2021-02-01;10;no_alarm;no_alarm;32;2019;1111-11-11 11:11.11

// Test: IzarWater5 izar 20e4ffde NOKEY
// Comment: Ensure non-regression on manufacture year parsing
// telegram=|1944304CDEFFE420CC01A2_63120013258F907B0AFF12529AC33B|
// {"media":"water","meter":"izar","name":"IzarWater5","id":"20e4ffde","prefix":"C15SA","serial_number":"007710","total_m3":159.832,"last_month_total_m3":157.76,"last_month_measure_date":"2021-02-01","remaining_battery_life_y":9,"current_alarms":"no_alarm","previous_alarms":"no_alarm","transmit_period_s":32,"manufacture_year":"2015","timestamp":"1111-11-11T11:11:11Z"}
// |IzarWater5;20e4ffde;C15SA;007710;159.832;157.76;2021-02-01;9;no_alarm;no_alarm;32;2015;1111-11-11 11:11.11

// Test: IzarWater6 izar 48500375 NOKEY
// telegram=|19442423860775035048A251520015BEB6B2E1ED623A18FC74A5|
// {"media":"water","meter":"izar","name":"IzarWater6","id":"48500375","total_m3":521.602,"last_month_total_m3":519.147,"last_month_measure_date":"2021-11-15","remaining_battery_life_y":9,"current_alarms":"no_alarm","previous_alarms":"leakage","transmit_period_s":8,"timestamp":"1111-11-11T11:11:11Z"}
// |IzarWater6;48500375;null;null;521.602;519.147;2021-11-15;9;no_alarm;leakage;8;null;1111-11-11 11:11.11
