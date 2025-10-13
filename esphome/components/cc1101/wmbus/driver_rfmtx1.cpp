/*
 Copyright (C) 2020 Fredrik Öhrström (gpl-3.0-or-later)

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
        di.setName("rfmtx1");
        di.setDefaultFields("name,id,total_m3,meter_datetime,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_BMT, 0x07,  0x05);
        di.usesProcessContent();
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addNumericFieldWithExtractor(
            "total",
            "The total water consumption recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            );

        addStringFieldWithExtractor(
            "meter_datetime",
            "Date time when meter sent this telegram.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime));
    }

    uchar decode_vectors_[16][6] = { { 117, 150, 122, 16, 26, 10 }, { 91, 127, 112, 19, 34, 19 }, { 179, 24, 185, 11, 142, 153 },
                                     { 142, 125, 121, 7, 74, 22 }, { 181, 145, 7, 154, 203, 105 }, { 184, 163, 50, 161, 57, 14 },
                                     { 189, 128, 156, 126, 96, 153 }, { 39, 92, 180, 196, 128, 163 }, { 48, 208, 10, 206, 25, 3 },
                                     { 194, 76, 240, 5, 165, 134 }, { 84, 75, 22, 152, 17, 94 }, { 75, 238, 12, 201, 125, 162 },
                                     { 135, 202, 74, 72, 228, 31 }, { 196, 135, 119, 46, 138, 232 }, { 227, 48, 189, 120, 87, 140 },
                                     { 164, 154, 57, 111, 40, 5 } };

    void Driver::processContent(Telegram *t)
    {
        if (t->tpl_cfg == 0x1006)
        {
            // This is the old type of meter and some values needs to be de-obfuscated.
            vector<uchar> frame;
            t->extractFrame(&frame);

            debugPayload("(rftx1) decoding raw frame", frame);

            uchar decoded_total[6];

            for (int i = 0; i < 6; ++i)
            {
                decoded_total[i] = (uchar)(frame[0xf + i] ^ frame[0xb] ^ decode_vectors_[frame[0xb] & 0x0f][i]);
            }

            double total = 0;
            double mul = 1;
            for (int i = 2; i < 6; ++i)
            {
                total += mul*bcd2bin(decoded_total[i]);
                mul *= 100;
            }
            setNumericValue("total", Unit::M3, total/1000.0);

            int o = 28; // Offset to datetime.
            int y = 2000+bcd2bin(frame[o+5]);
            int M = bcd2bin(frame[o+4]);
            int d = bcd2bin(frame[o+3]);
            int H = bcd2bin(frame[o+2]);
            int m = bcd2bin(frame[o+1]);
            int s = bcd2bin(frame[o+0]);

            string tmp;
            strprintf(&tmp, "%d-%02d-%02d %02d:%02d:%02d",y, M%99, d%99, H%99, m%99, s%99);
            setStringValue("meter_datetime", tmp);

            return;
        }
    }
}

// Test: Wasser rfmtx1 74737271 NOKEY
// telegram=|4644B4097172737405077AA5000610_1115F78184AB0F1D1E200000005904103103208047004A4800E73C00193E00453F003E4000E64000E74100F442000144001545005B460000|
// {"media":"water","meter":"rfmtx1","name":"Wasser","id":"74737271","total_m3":188.56,"meter_datetime":"2020-03-31 10:04:59","timestamp":"1111-11-11T11:11:11Z"}
// |Wasser;74737271;188.56;2020-03-31 10:04:59;1111-11-11 11:11.11
