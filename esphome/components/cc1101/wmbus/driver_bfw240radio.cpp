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

        void processContent(Telegram *t);
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("bfw240radio");
        di.setDefaultFields("name,id,current_hca,prev_hca,timestamp");
        di.addLinkMode(LinkMode::T1);
        di.setMeterType(MeterType::HeatCostAllocationMeter);
        di.addDetection(MANUFACTURER_BFW,0x08,  0x02);
        di.forceMfctIndex(2); // First two bytes are 2f2f after that its completely mfct specific.
        di.usesProcessContent();
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addNumericField("current",
                        Quantity::HCA,
                        DEFAULT_PRINT_PROPERTIES,
                        "Energy consumption so far in this billing period.");

        addNumericField("prev",
                        Quantity::HCA,
                        DEFAULT_PRINT_PROPERTIES,
                        "Energy consumption at end of previous billing period.");

        for (int i=0; i<18; ++i)
        {
            string info = tostrprintf("prev_%02d", i+1);
            string about = tostrprintf("Energy consumption %d months ago.", i+1);

            addNumericField(info,
                            Quantity::HCA,
                            DEFAULT_PRINT_PROPERTIES,
                            about);
        }

        addStringField("device_date",
                       "Device date when telegram was sent.",
                       DEFAULT_PRINT_PROPERTIES);
    }


    int getHistoric(int n, vector<uchar> &content)
    {
        assert(n >= 0 && n < 18);
        assert(content.size() >= 40);

        int offset = (n*12)/8;
        int remainder = (n*12)%8;

        uchar lo, hi;

        if (remainder == 0)
        {
            lo = content[36-offset];
            hi = 0x0f & content[36-1-offset];
        }
        else
        {
            assert(remainder == 4);
            lo = content[36-1-offset];
            hi = (0xf0 & content[36-offset]) >> 4;
        }
        return hi*256 + lo;
    }

    /*
    date of telegram reception--------------------------------------------------------------------------------|
                                                                                                              |
    18 historic monthly values (newest to the right, byte-reordering for 2.,4.,6.,...-oldest month)----|      |
                                                                                                       |      |
    ???------------------------|                                                                       |      |
                               |                                                                       |      |
    current consumption---|    |                                                                       |      |
                          |    |                                                                       |      |
    prev. cons.---vvvv vvvv vvvv vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv vvvvvv
    feb: 2F2F6F1F 0144 0100 1470 000 000 000 000 000 000 000 000 000 000 000 000 000 000 370 09B 441 0AC 260221
    mar: 2F2F481F 0144 0100 1470 000 000 000 000 000 000 000 000 000 000 000 000 000 037 9B0 144 AC0 100 040321
    apr: 2F2F871F 0144 013C 1470 000 000 000 000 000 000 000 000 000 000 000 000 370 09B 441 0AC 001 13C 030421
    */
    void Driver::processContent(Telegram *t)
    {
        vector<uchar> content;
        t->extractPayload(&content);

        if (content.size() < 40) return;

        double current_hca = content[6]*256 + content[7];
        setNumericValue("current", Unit::HCA, current_hca);

        string msg = tostrprintf("*** %02X%02X \"current_hca\":%g", content[6], content[7], current_hca);
        t->addSpecialExplanation(6+t->header_size, 2, KindOfData::CONTENT, Understanding::FULL, msg.c_str());

        double prev_hca = content[4]*256 + content[5];
        setNumericValue("prev", Unit::HCA, prev_hca);

        msg = tostrprintf("*** %02X%02X \"prev_hca\":%g", content[4], content[5], prev_hca);
        t->addSpecialExplanation(4+t->header_size, 2, KindOfData::CONTENT, Understanding::FULL, msg.c_str());

        string device_date = tostrprintf("20%02x-%02x-%02x", content[39], content[39-1], content[39-2]);
        setStringValue("device_date", device_date, NULL);

        msg = tostrprintf("*** %02X%02X%02X \"device_date\":\"%s\"", content[39-2], content[39-1], content[39],
                                 device_date.c_str());
        t->addSpecialExplanation(39-2+t->header_size, 3, KindOfData::CONTENT, Understanding::FULL, msg.c_str());


        for (int i=0; i<18; ++i)
        {
            string info = tostrprintf("prev_%02d", i+1);
            double historic_hca = getHistoric(i, content);
            setNumericValue(info, Unit::HCA, historic_hca);
        }
    }
}

// Test: bfw bfw240radio 00707788 NOKEY
// telegram=|3644D7088877700002087ADBC000002F2F9E1F03C10388152A00000000000000000000000000000204000404000EE2020AC1321D280221|
// {"media":"heat cost allocation","meter":"bfw240radio","name":"bfw","id":"00707788","current_hca":904,"prev_hca":961,"prev_01_hca":541,"prev_02_hca":961,"prev_03_hca":522,"prev_04_hca":226,"prev_05_hca":14,"prev_06_hca":4,"prev_07_hca":4,"prev_08_hca":4,"prev_09_hca":2,"prev_10_hca":0,"prev_11_hca":0,"prev_12_hca":0,"prev_13_hca":0,"prev_14_hca":0,"prev_15_hca":0,"prev_16_hca":0,"prev_17_hca":0,"prev_18_hca":0,"device_date":"2021-02-28","timestamp":"1111-11-11T11:11:11Z"}
// |bfw;00707788;904;961;1111-11-11 11:11.11

// telegram=|3644D7088877700002087A8BC000002F2F011F03C1038D152A0000000000000000000000000200040400040E00E20A23C11D238D010321|
// {"media":"heat cost allocation","meter":"bfw240radio","name":"bfw","id":"00707788","current_hca":909,"prev_hca":961,"prev_01_hca":909,"prev_02_hca":541,"prev_03_hca":961,"prev_04_hca":522,"prev_05_hca":226,"prev_06_hca":14,"prev_07_hca":4,"prev_08_hca":4,"prev_09_hca":4,"prev_10_hca":2,"prev_11_hca":0,"prev_12_hca":0,"prev_13_hca":0,"prev_14_hca":0,"prev_15_hca":0,"prev_16_hca":0,"prev_17_hca":0,"prev_18_hca":0,"device_date":"2021-03-01","timestamp":"1111-11-11T11:11:11Z"}
// |bfw;00707788;909;961;1111-11-11 11:11.11

// Test: bfww bfw240radio 00707076 NOKEY
// telegram=|3644D7087670700002087A9CC000002F2F6E1F000000000B36000000000000000000000000000000000000000000000000000000260221|
// {"media":"heat cost allocation","meter":"bfw240radio","name":"bfww","id":"00707076","current_hca":0,"prev_hca":0,"prev_01_hca":0,"prev_02_hca":0,"prev_03_hca":0,"prev_04_hca":0,"prev_05_hca":0,"prev_06_hca":0,"prev_07_hca":0,"prev_08_hca":0,"prev_09_hca":0,"prev_10_hca":0,"prev_11_hca":0,"prev_12_hca":0,"prev_13_hca":0,"prev_14_hca":0,"prev_15_hca":0,"prev_16_hca":0,"prev_17_hca":0,"prev_18_hca":0,"device_date":"2021-02-26","timestamp":"1111-11-11T11:11:11Z"}
// |bfww;00707076;0;0;1111-11-11 11:11.11

// telegram=|3644D7087670700002087A27C000002F2F011F000000000B36000000000000000000000000000000000000000000000000000000010321|
// {"media":"heat cost allocation","meter":"bfw240radio","name":"bfww","id":"00707076","current_hca":0,"prev_hca":0,"prev_01_hca":0,"prev_02_hca":0,"prev_03_hca":0,"prev_04_hca":0,"prev_05_hca":0,"prev_06_hca":0,"prev_07_hca":0,"prev_08_hca":0,"prev_09_hca":0,"prev_10_hca":0,"prev_11_hca":0,"prev_12_hca":0,"prev_13_hca":0,"prev_14_hca":0,"prev_15_hca":0,"prev_16_hca":0,"prev_17_hca":0,"prev_18_hca":0,"device_date":"2021-03-01","timestamp":"1111-11-11T11:11:11Z"}
// |bfww;00707076;0;0;1111-11-11 11:11.11


// Test: bfwww bfw240radio 00707447 NOKEY
// telegram=|3644D7084774700002087A80C000002F2F6F1F01440100147000000000000000000000000000000000000000000037009B4410AC260221|
// {"media":"heat cost allocation","meter":"bfw240radio","name":"bfwww","id":"00707447","current_hca":256,"prev_hca":324,"prev_01_hca":172,"prev_02_hca":324,"prev_03_hca":155,"prev_04_hca":55,"prev_05_hca":0,"prev_06_hca":0,"prev_07_hca":0,"prev_08_hca":0,"prev_09_hca":0,"prev_10_hca":0,"prev_11_hca":0,"prev_12_hca":0,"prev_13_hca":0,"prev_14_hca":0,"prev_15_hca":0,"prev_16_hca":0,"prev_17_hca":0,"prev_18_hca":0,"device_date":"2021-02-26","timestamp":"1111-11-11T11:11:11Z"}
// |bfwww;00707447;256;324;1111-11-11 11:11.11

// telegram=|3644D7084774700002087AE1C000002F2F481F0144010014700000000000000000000000000000000000000000379B0144AC0100040321|
// {"media":"heat cost allocation","meter":"bfw240radio","name":"bfwww","id":"00707447","current_hca":256,"prev_hca":324,"prev_01_hca":256,"prev_02_hca":172,"prev_03_hca":324,"prev_04_hca":155,"prev_05_hca":55,"prev_06_hca":0,"prev_07_hca":0,"prev_08_hca":0,"prev_09_hca":0,"prev_10_hca":0,"prev_11_hca":0,"prev_12_hca":0,"prev_13_hca":0,"prev_14_hca":0,"prev_15_hca":0,"prev_16_hca":0,"prev_17_hca":0,"prev_18_hca":0,"device_date":"2021-03-04","timestamp":"1111-11-11T11:11:11Z"}
// |bfwww;00707447;256;324;1111-11-11 11:11.11
