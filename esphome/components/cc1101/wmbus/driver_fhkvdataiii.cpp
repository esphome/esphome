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

    private:

        void processContent(Telegram *t);
        string leadingZeroString(int num);
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("fhkvdataiii");
        di.setDefaultFields("name,id,current_hca,current_date,previous_hca,previous_date,temp_room_c,temp_radiator_c,timestamp");
        di.setMeterType(MeterType::HeatCostAllocationMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_TCH, 0x80,  0x69);
        di.addDetection(MANUFACTURER_TCH, 0x80,  0x94);
        di.usesProcessContent();
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addNumericField("current",
                        Quantity::HCA,
                        DEFAULT_PRINT_PROPERTIES,
                        "Energy consumption so far in this billing period.");

        addStringField("current_date",
                       "Date of current billing period.",
                       DEFAULT_PRINT_PROPERTIES);

        addNumericField("previous",
                        Quantity::HCA,
                        DEFAULT_PRINT_PROPERTIES,
                        "Energy consumption in previous billing period.");

        addStringField("previous_date",
                       "Date of last billing period.",
                       DEFAULT_PRINT_PROPERTIES);

        addNumericField("temp_room",
                        Quantity::Temperature,
                        DEFAULT_PRINT_PROPERTIES,
                        "Current room temperature.");

        addNumericField("temp_radiator",
                        Quantity::Temperature,
                        DEFAULT_PRINT_PROPERTIES,
                        "Current radiator temperature.");
    }

    void Driver::processContent(Telegram *t)
    {
        // The Techem FHKV data ii/iii is mostly a proprietary protocol
        // simple wrapped inside a wmbus telegram since the ci-field is 0xa0.
        // Which means that the entire payload is manufacturer specific.

        vector<uchar> content;

        t->extractPayload(&content);

        if (content.size() < 14)
        {
            // Not enough data.
            debugPayload("(fhkvdataiii) not enough data", content);
            return;
        }


        // Previous Date ////////////////////////////////////////////////////////

        uchar date_prev_lo = content[1];
        uchar date_prev_hi = content[2];
        int date_prev = (256.0*date_prev_hi+date_prev_lo);

        int day_prev = (date_prev >> 0) & 0x1F;
        int month_prev = (date_prev >> 5) & 0x0F;
        int year_prev = 2000 + ((date_prev >> 9) & 0x3F);

        string previous_date =
            std::to_string(year_prev) + "-" +
            leadingZeroString(month_prev) + "-" +
            leadingZeroString(day_prev) + "T02:00:00Z";

        setStringValue("previous_date", previous_date, NULL);

        string bytes = tostrprintf("%02x%02x", content[1], content[2]);
        string info = "*** "+bytes+" previous_date = %s";

        t->addSpecialExplanation(t->header_size+1, 2, KindOfData::CONTENT, Understanding::FULL,
                                 info.c_str(), previous_date.c_str());

        // Previous Consumption //////////////////////////////////////////////////////////

        uchar prev_lo = content[3];
        uchar prev_hi = content[4];
        double prev = (256.0*prev_hi+prev_lo);
        double previous_hca = prev;
        setNumericValue("previous", Unit::HCA, previous_hca);

        bytes = tostrprintf("%02x%02x", content[3], content[4]);
        info = "*** "+bytes+" previous_hca = %.17g";

        t->addSpecialExplanation(t->header_size+3, 2, KindOfData::CONTENT, Understanding::FULL,
                                 info.c_str(), previous_hca);

        // Current Date //////////////////////////////////////////////////////////////////

        uchar date_curr_lo = content[5];
        uchar date_curr_hi = content[6];
        int date_curr = (256.0*date_curr_hi+date_curr_lo);

        int day_curr = (date_curr >> 4) & 0x1F;
        if (day_curr <= 0) day_curr = 1;
        int month_curr = (date_curr >> 9) & 0x0F;
        if (month_curr <= 0) month_curr = 12;
        int year_curr = year_prev;
        if (month_curr < month_prev || (month_curr == month_prev && day_curr <= day_prev)) {
            year_curr++;
        }

        string current_date =
            std::to_string(year_curr) + "-" +
            leadingZeroString(month_curr) + "-" +
            leadingZeroString(day_curr) + "T02:00:00Z";

        setStringValue("current_date", current_date, NULL);

        bytes = tostrprintf("%02x%02x", content[5], content[6]);
        info = "*** "+bytes+" current_date = %s";

        t->addSpecialExplanation(t->header_size+5, 2, KindOfData::CONTENT, Understanding::FULL,
                                 info.c_str(), current_date.c_str());

        // Current Consumption ///////////////////////////////////////////////////////////

        uchar curr_lo = content[7];
        uchar curr_hi = content[8];
        double current_hca = (256.0*curr_hi+curr_lo);

        setNumericValue("current", Unit::HCA, current_hca);

        bytes = tostrprintf("%02x%02x", content[7], content[8]);
        info = "*** "+bytes+" current_hca = %.17g";

        t->addSpecialExplanation(t->header_size+7, 2, KindOfData::CONTENT, Understanding::FULL,
                                 info.c_str(), current_hca);

        // Temperatures //////////////////////////////////////////////

        uchar room_tlo;
        uchar room_thi;
        uchar radiator_tlo;
        uchar radiator_thi;
        int offset = 9;

        if(t->dll_version == 0x94)
        {
            offset= 10;
        }

        room_tlo = content[offset];
        room_thi = content[offset+1];
        radiator_tlo = content[offset+2];
        radiator_thi = content[offset+3];

        // Room Temperature ////////////////////////////////////////////////////////////////

        double temp_room_c = (256.0*room_thi+room_tlo)/100;
        setNumericValue("temp_room", Unit::C, temp_room_c);

        bytes = tostrprintf("%02x%02x", content[offset], content[offset+1]);
        info = "*** "+bytes+" temp_room_c = %.17g";

        t->addSpecialExplanation(t->header_size+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                 info.c_str(), temp_room_c);

        // Radiator Temperature ////////////////////////////////////////////////////////////

        double temp_radiator_c = (256.0*radiator_thi+radiator_tlo)/100;
        setNumericValue("temp_radiator", Unit::C, temp_radiator_c);

        bytes = tostrprintf("%02x%02x", content[offset+2], content[offset+3]);
        info = "*** "+bytes+" temp_radiator_c = %.17g";

        t->addSpecialExplanation(t->header_size+offset+2, 2, KindOfData::CONTENT, Understanding::FULL,
                                 info.c_str(), temp_radiator_c);
    }

    string Driver::leadingZeroString(int num) {
        string new_num = (num < 10 ? "0": "") + std::to_string(num);
        return new_num;
    }
}

// Test: Room fhkvdataiii 11776622 NOKEY
// telegram=|31446850226677116980A0119F27020480048300C408F709143C003D341A2B0B2A0707000000000000062D114457563D71A1850000|
// {"media":"heat cost allocator","meter":"fhkvdataiii","name":"Room","id":"11776622","current_hca":131,"current_date":"2020-02-08T02:00:00Z","previous_hca":1026,"previous_date":"2019-12-31T02:00:00Z","temp_room_c":22.44,"temp_radiator_c":25.51,"timestamp":"1111-11-11T11:11:11Z"}
// |Room;11776622;131;2020-02-08T02:00:00Z;1026;2019-12-31T02:00:00Z;22.44;25.51;1111-11-11 11:11.11

// Test: Rooom fhkvdataiii 11111234 NOKEY
// telegram=|33446850341211119480A2_0F9F292D005024040011BD08380904000000070000000000000000000000000001000000000003140E|
// {"media":"heat cost allocator","meter":"fhkvdataiii","name":"Rooom","id":"11111234","current_hca":4,"current_date":"2021-02-05T02:00:00Z","previous_hca":45,"previous_date":"2020-12-31T02:00:00Z","temp_room_c":22.37,"temp_radiator_c":23.6,"timestamp":"1111-11-11T11:11:11Z"}
// |Rooom;11111234;4;2021-02-05T02:00:00Z;45;2020-12-31T02:00:00Z;22.37;23.6;1111-11-11 11:11.11
