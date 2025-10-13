/*
 Copyright (C) 2017-2022 Fredrik Öhrström (gpl-3.0-or-later)

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
#include "Telegram.h"
#include <cmath>

namespace
{
    struct Driver : public virtual MeterCommonImplementation
    {
        Driver(MeterInfo &mi, DriverInfo &di);

        void processContent(Telegram *t);
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("apatorna1");
        di.setDefaultFields("name,id,total_m3,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addDetection(MANUFACTURER_APA,  0x07,  0x14);
        di.usesProcessContent();
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addNumericField(
            "total",
            Quantity::Volume,
            DEFAULT_PRINT_PROPERTIES,
            "The total water consumption recorded by this meter.");
    }

    void Driver::processContent(Telegram *t)
    {
        vector<uchar> content;
        t->extractPayload(&content);

        if (content.size() < 4) return;

        vector<uchar> frame(content.begin() + 2, content.begin() + 18);
        vector<uchar>::iterator pos = frame.begin();

        // TODO: read specified key from input
        vector<uchar> aes_key(16, 0);

        int num_encrypted_bytes = 0;
        int num_not_encrypted_at_end = 0;

        t->tpl_acc = content[0]; //0xBB;

        decrypt_TPL_AES_CBC_IV(t, frame, pos, aes_key, &num_encrypted_bytes, &num_not_encrypted_at_end);

        const int multiplier = pow(10, (frame.at(1) & 0b00110000) >> 4);

        const int reading = static_cast<int>(frame.at(4)) << 20 |
                            static_cast<int>(frame.at(3)) << 12 |
                            static_cast<int>(frame.at(2)) << 4  |
                            (static_cast<int>(frame.at(1)) & 0b00001111);

        const double volume = static_cast<double>(reading) * multiplier / 1000;

        setNumericValue("total", Unit::M3, volume);
    }
}

// Test: ApNa1 apatorna1 04913581 00000000000000000000000000000000
// telegram=|1C440106813591041407A0B000266A705474DDB80D9A0EB9AE2EF29D96|
// {"media":"water","meter":"apatorna1","name":"ApNa1","id":"04913581","total_m3":345.312,"timestamp":"1111-11-11T11:11:11Z"}
// |ApNa1;04913581;345.312;1111-11-11 11:11.11
