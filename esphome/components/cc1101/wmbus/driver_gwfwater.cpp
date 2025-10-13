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
        di.setName("gwfwater");
        di.setDefaultFields("name,id,total_m3,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addDetection(MANUFACTURER_GWF, 0x0e,  0x01);
        di.usesProcessContent();
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addOptionalLibraryFields("actuality_duration_s");
        addOptionalLibraryFields("total_m3,target_m3,target_date");

        addStringField(
            "status",
            "Meter status.",
            PrintProperty::INCLUDE_TPL_STATUS | PrintProperty::STATUS);

        addStringField(
            "mfct_status",
            "Mfct status bit.",
            PrintProperty::INJECT_INTO_STATUS | PrintProperty::HIDE);

        addStringField(
            "power_mode",
            "Normal or saving.",
            DEFAULT_PRINT_PROPERTIES);

        addNumericField(
            "battery",
            Quantity::Time,
            DEFAULT_PRINT_PROPERTIES,
            "Remaining battery life in years.",
            Unit::Year);
    }

    void Driver::processContent(Telegram *t)
    {
        vector<uchar> bytes;
        t->extractMfctData(&bytes); // Extract raw frame data after the DIF 0x0F.

        if (bytes.size() < 3) return;

        uchar type = bytes[0];
        uchar a = bytes[1];
        uchar b = bytes[2];

        if (type != 1)
        {
            setStringValue("mfct_status", tostrprintf("UKNOWN_MFCT_STATUS=%02x%02x%02x", type, a, b), NULL);
            return;
        }

        string info;

        if (a & 0x02) info += "CONTINUOUS_FLOW ";
        if (a & 0x08) info += "BROKEN_PIPE ";
        if (a & 0x20) info += "BATTERY_LOW ";
        if (a & 0x40) info += "BACKFLOW ";

        if (info.size() > 0) info.pop_back();
        setStringValue("mfct_status", info, NULL);

        setStringValue("power_mode", (b & 0x01) ? "SAVING" :  "NORMAL", NULL);

        double battery_semesters = (b >> 3); // Half years.
        setNumericValue("battery", Unit::Year, battery_semesters/2.0);
    }
}

// Test: Wateroo gwfwater 20221031 NOKEY
// telegram=|3144E61E31102220010E8C04F47ABE0420452F2F_037410000004133E0000004413FFFFFFFF426CFFFF0F0120012F2F2F2F2F|
// {"actuality_duration_s": 16,"battery_y": 0,"id": "20221031","media": "bus/system component","meter": "gwfwater","name": "Wateroo","power_mode": "SAVING","status": "BATTERY_LOW POWER_LOW","target_date": "2128-03-31","target_m3": -0.001,"timestamp": "1111-11-11T11:11:11Z","total_m3": 0.062}
// |Wateroo;20221031;0.062;1111-11-11 11:11.11
