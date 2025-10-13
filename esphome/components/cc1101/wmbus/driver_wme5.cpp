/*
 Copyright (C) 2023 Fredrik Öhrström (gpl-3.0-or-later)

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
        di.setName("wme5");
        di.setDefaultFields("name,id,total_m3,total_hex,timestamp");

        di.setMeterType(MeterType::HeatMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_QDS, 0x07, 0x1a);
        di.usesProcessContent();
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addOptionalLibraryFields("meter_datetime,model_version");

        addNumericField(
            "total",
            Quantity::Volume,
            DEFAULT_PRINT_PROPERTIES,
            "Perhaps the total water consumption recorded by this meter.");

        addStringField(
            "total_hex",
            "Perhaps the total but in hex?",
            DEFAULT_PRINT_PROPERTIES);
    }

    void Driver::processContent(Telegram *t)
    {
        string content;
        int offset;
        bool ok = extractDVHexString(&t->dv_entries,
                                     "0DFF5F",
                                     &offset,
                                     &content);

        if (!ok) return;

        // 00826100 _ 35AE6A130B8A8CF07C0C6F9EA35C8C5274671347D73DA9810CD664F2F9616388CE7B4835BD06D7E2253741F2667DC5D8C
        if (content.length() >= 8)
        {
            vector<uchar> bytes;
            content = content.substr(0, 8);
            ok = hex2bin(content, &bytes);
            if (!ok) return;
            if (bytes.size() != 4) return;

            uint64_t total = bytes[0]<<24 | bytes[1] << 16 | bytes [2] << 8 | bytes[3];
            double totald = total;
            setNumericValue("total", Unit::M3, totald);
            setStringValue("total_hex", content);
        }
    }

}
