/*
 Copyright (C) 2020-2022 Fredrik Öhrström (gpl-3.0-or-later)

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

 @see https://www.lansensystems.com/media/1282/mbus_data_format_lan-wmbus-r4_v11_rev_3.pdf
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
        di.setName("lansenrp");
        di.setDefaultFields("name,id,status,total_routed_messages_counter,used_router_slots_counter,is_repeater_listening,timestamp");
        di.setMeterType(MeterType::Repeater);
        di.addLinkMode(LinkMode::C1);
        di.addDetection(MANUFACTURER_LAS, 0x32, 0x0b);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        setMfctTPLStatusBits(
            Translate::Lookup()
            .add(Translate::Rule("TPL_STS", Translate::MapType::BitToString)
                 .set(MaskBits(0xe0))
                 .set(DefaultMessage("OK"))
                 .add(Translate::Map(0x04 ,"LOW_BATTERY", TestBit::Set))));

        addStringField(
            "status",
            "Meter status from tpl status field.",
            DEFAULT_PRINT_PROPERTIES   |
            PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS);

        addNumericFieldWithExtractor(
            "total_routed_messages",
            "Number of total routed messages since power up",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Dimensionless)
            );

        addNumericFieldWithExtractor(
            "used_router_slots",
            "Used router slots (maximum 936)",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Dimensionless)
            .set(SubUnitNr(1))
            );

        addStringFieldWithExtractor(
            "software_version",
            "Software version of repeater",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::SoftwareVersion)
            );

        addStringFieldWithExtractorAndLookup(
            "is_repeater_listening",
            "Is the repeater listening (YES/NO)",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Dimensionless)
            .set(SubUnitNr(2)),
            Translate::Lookup()
            .add(Translate::Rule("INPUT_BITS", Translate::MapType::IndexToString)
                 .set(MaskBits(0x01))
                 .add(Translate::Map(0x00, "NO", TestBit::Set))
                 .add(Translate::Map(0x01, "YES", TestBit::Set))
                ));

        addNumericFieldWithExtractor(
            "seconds_to_mode_change",
            "Seconds to mode change (Listen -> Sleep or Sleep -> Listen). Maximum 32767 seconds",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Dimensionless)
            .set(SubUnitNr(3))
            );

        addNumericFieldWithExtractor(
            "listen_timer_value",
            "Value on parameter 'Listen timer'",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Dimensionless)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "pause_timer_value",
            "Value on parameter 'Pause timer'",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Dimensionless)
            .set(StorageNr(2))
            );

        addStringFieldWithExtractorAndLookup(
            "repeater_listening_on_weekdays",
            "Shows which weekday(s) repeater is listening (MO/TU/WE/TH/FR/SA/SU)",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Dimensionless)
            .set(StorageNr(3)),
            Translate::Lookup()
            .add(Translate::Rule("INPUT_BITS", Translate::MapType::BitToString)
                 .set(MaskBits(0xffff))
                 .add(Translate::Map(0x01 ,"SU", TestBit::Set))
                 .add(Translate::Map(0x02 ,"MO", TestBit::Set))
                 .add(Translate::Map(0x04 ,"TU", TestBit::Set))
                 .add(Translate::Map(0x08 ,"WE", TestBit::Set))
                 .add(Translate::Map(0x10 ,"TH", TestBit::Set))
                 .add(Translate::Map(0x20 ,"FR", TestBit::Set))
                 .add(Translate::Map(0x40 ,"SA", TestBit::Set))
                ));

        addNumericFieldWithExtractor(
            "start_time_value",
            "Value on parameter 'Start time', shown as minusted after midnight (-1=Not used)",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Dimensionless)
            .set(StorageNr(4))
            );

        addStringFieldWithExtractor(
            "meter_datetime",
            "Date and time when the meter sent the telegram.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            );

        addNumericFieldWithExtractor(
            "battery",
            "Battery voltage.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Voltage)
            );
    }
}

// Test: REPEAT lansenrp 00035946 NOKEY
// telegram=|54443330465903000B327A2B0000402F2F04FD3A946709008240FD3A600002FD0F9500818040FD3A0084C040FD3A8838000042FD3A28008201FD3A8C05C101FD3A7F8202FD3A3804066D35122EFB2B0002FD46D00C|
// {"media":"reserved","meter":"lansenrp","name":"REPEAT","id":"00035946","battery_v":3.28,"listen_timer_value_counter":40,"pause_timer_value_counter":1420,"seconds_to_mode_change_counter":14472,"start_time_value_counter":1080,"total_routed_messages_counter":616340,"used_router_slots_counter":96,"is_repeater_listening":"NO","meter_datetime":"2023-11-27 14:18:53","repeater_listening_on_weekdays":"FR MO SA SU TH TU WE","software_version":"0095","status":"OK","timestamp":"1111-11-11T11:11:11Z"}
// |REPEAT;00035946;OK;616340;96;NO;1111-11-11 11:11.11

// telegram=|54443330465903000B327A2B0400402F2F04FD3A946709008240FD3A600002FD0F9500818040FD3A0184C040FD3A8838000042FD3A28008201FD3A8C05C101FD3A088202FD3A3804066D35122EFB2B0002FD46D00C|
// {"media":"reserved","meter":"lansenrp","name":"REPEAT","id":"00035946","battery_v":3.28,"listen_timer_value_counter":40,"pause_timer_value_counter":1420,"seconds_to_mode_change_counter":14472,"start_time_value_counter":1080,"total_routed_messages_counter":616340,"used_router_slots_counter":96,"is_repeater_listening":"YES","meter_datetime":"2023-11-27 14:18:53","repeater_listening_on_weekdays":"WE","software_version":"0095","status":"POWER_LOW","timestamp":"1111-11-11T11:11:11Z"}
// |REPEAT;00035946;POWER_LOW;616340;96;YES;1111-11-11 11:11.11
