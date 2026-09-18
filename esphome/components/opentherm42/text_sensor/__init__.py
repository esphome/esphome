import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from .. import OpenTherm42Hub
from ..const import (
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_CH2_PRESENT,
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_CONTROL_TYPE,
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_COOLING_CONFIG,
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_DHW_CONFIG,
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_DHW_PRESENT,
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_HEAT_COOL_MODE_CONTROL,
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_MASTER_LOW_OFF_AND_PUMP_CONTROL_FUNCTION,
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_REMOTE_WATER_FILLING_FUNCTION,
    CONF_CONFIGURATION_INFORMATION_BRAND,
    CONF_CONFIGURATION_INFORMATION_BRAND_SERIAL_NUMBER,
    CONF_CONFIGURATION_INFORMATION_BRAND_VERSION,
    CONF_CONFIGURATION_INFORMATION_CONFIGURATION_VENTILATION_HEAT_RECOVERY_BYPASS,
    CONF_CONFIGURATION_INFORMATION_CONFIGURATION_VENTILATION_HEAT_RECOVERY_SPEED_CONTROL,
    CONF_CONFIGURATION_INFORMATION_CONFIGURATION_VENTILATION_HEAT_RECOVERY_SYSTEM_TYPE,
    CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_CONFIGURATION_SYSTEM_TYPE,
    CONF_CONTROL_AND_STATUS_INFORMATION_SOLAR_STORAGE_MODE_AND_STATUS_SOLAR_MODE,
    CONF_CONTROL_AND_STATUS_INFORMATION_SOLAR_STORAGE_MODE_AND_STATUS_SOLAR_STATUS,
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_OPERATING_MODE_DHW,
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_OPERATING_MODE_HEATING_HC1,
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_OPERATING_MODE_HEATING_HC2,
    CONF_OPENTHERM42_ID,
    CONF_REMOTE_REQUEST_LAST_RESPONSE,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DATE_TIME,
)

# device_class is left unset throughout: Home Assistant text sensors only support "date" and
# "timestamp", and none of these are that.
TYPES: dict[str, cv.Schema] = {
    # §5.3.2 Class 2, ID 93: Brand -- the boiler manufacturer's brand name, read one ASCII character
    # at a time (index in HB, character in LB) until the boiler-reported character count is reached.
    # entity_category is DIAGNOSTIC: a static Class 2 identification string the boiler reports once,
    # the same nature as HA's own example of a diagnostic entity ("a sensor showing... MAC address").
    CONF_CONFIGURATION_INFORMATION_BRAND: text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.2 Class 2, ID 94: Brand version, read the same way as ID 93.
    CONF_CONFIGURATION_INFORMATION_BRAND_VERSION: text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.2 Class 2, ID 95: Brand serial number, read the same way as ID 93.
    CONF_CONFIGURATION_INFORMATION_BRAND_SERIAL_NUMBER: text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.2 Class 2, ID 3 HB bit 0: DHW present [ dhw not present, dhw is present ] -- a small named
    # enum (each bit is really a 2-state code), so a text_sensor showing the spec's own wording
    # rather than a bare on/off. Same DIAGNOSTIC nature as the brand strings above: a static
    # capability flag the boiler reports once, not watched day-to-day.
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_DHW_PRESENT: text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.2 Class 2, ID 3 HB bit 1: Control type [ modulating, on/off ].
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_CONTROL_TYPE: text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.2 Class 2, ID 3 HB bit 2: Cooling config [ cooling not supported, cooling supported ].
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_COOLING_CONFIG: text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.2 Class 2, ID 3 HB bit 3: DHW config [ instantaneous or not-specified, storage tank ].
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_DHW_CONFIG: text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.2 Class 2, ID 3 HB bit 4: Master low-off&pump control function [ allowed, not allowed ].
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_MASTER_LOW_OFF_AND_PUMP_CONTROL_FUNCTION: text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.2 Class 2, ID 3 HB bit 5: CH2 present [ CH2 not present, CH2 present ].
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_CH2_PRESENT: text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.2 Class 2, ID 3 HB bit 6: Remote water filling function [ available or unknown, not
    # available ]. Unknown for applications with protocol version 2.2 or older.
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_REMOTE_WATER_FILLING_FUNCTION: text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.2 Class 2, ID 3 HB bit 7: Heat/cool mode control [ switching done by master, switching
    # done by boiler ].
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_HEAT_COOL_MODE_CONTROL: text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.2 Class 2, ID 74 HB bit 0: System type [ 0 = central exhaust ventilation, 1 = heat-recovery
    # ventilation ].
    CONF_CONFIGURATION_INFORMATION_CONFIGURATION_VENTILATION_HEAT_RECOVERY_SYSTEM_TYPE: text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.2 Class 2, ID 74 HB bit 1: Bypass [ not present, present ].
    CONF_CONFIGURATION_INFORMATION_CONFIGURATION_VENTILATION_HEAT_RECOVERY_BYPASS: text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.2 Class 2, ID 74 HB bit 2: Speed control [ 3-speed, variable ].
    CONF_CONFIGURATION_INFORMATION_CONFIGURATION_VENTILATION_HEAT_RECOVERY_SPEED_CONTROL: text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.2 Class 2, ID 103 HB bit 0: Solar Storage configuration: system type
    # [ 0 = DHW preheat system, 1 = DHW parallel system ].
    CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_CONFIGURATION_SYSTEM_TYPE: text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.3 Class 3, ID 4 LB: the most recent Request-Response-Code's meaning (0..127 = request
    # refused, 128..255 = request accepted), alongside the raw code exposed by the sensor platform's
    # remote_request_last_response_code. Feedback for the Class 3 remote-request buttons (all
    # entity_category CONFIG) -- not a value anyone watches day-to-day, so DIAGNOSTIC.
    CONF_REMOTE_REQUEST_LAST_RESPONSE: text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.1 Class 1, ID 101 LB bits 3,2,1: Solar Storage mode and status: Solar mode -- the
    # boiler's own reported mode (Off/DHW Eco/DHW Comfort/DHW Single Boost/DHW Continuous Boost),
    # a small named enum, so a text_sensor rather than a raw numeric code. Independent of the
    # select platform's master_solar_storage_status_solar_mode (ID 101 HB) -- see hub.h.
    CONF_CONTROL_AND_STATUS_INFORMATION_SOLAR_STORAGE_MODE_AND_STATUS_SOLAR_MODE: text_sensor.text_sensor_schema(),
    # §5.3.1 Class 1, ID 101 LB bits 5,4: Solar Storage mode and status: Solar status (Standby/
    # Loading By Sun/Loading By Boiler/Anti-Legionella).
    CONF_CONTROL_AND_STATUS_INFORMATION_SOLAR_STORAGE_MODE_AND_STATUS_SOLAR_STATUS: text_sensor.text_sensor_schema(),
    # §5.3.8.3 Class 8, ID 99 HB bits 0-3: Remote Override Operating Mode DHW -- read-only (per the
    # spec's own note: "the master can read on Data ID 99 the remote override Operating Modes"; the
    # only master-written part of this id is bit 4, Manual DHW push2, already a button).
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_OPERATING_MODE_DHW: text_sensor.text_sensor_schema(),
    # §5.3.8.3 Class 8, ID 99 LB bits 0-3: Remote Override Operating Mode Heating HC1 -- same
    # read-only nature as the DHW mode above, but its own distinct 7-state enum (Comfort here is
    # state 2, not Anti-Legionella; Precomfort exists here, not in the DHW variant).
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_OPERATING_MODE_HEATING_HC1: text_sensor.text_sensor_schema(),
    # §5.3.8.3 Class 8, ID 99 LB bits 4-7: Remote Override Operating Mode Heating HC2, same encoding
    # as HC1.
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_OPERATING_MODE_HEATING_HC2: text_sensor.text_sensor_schema(),
    # §5.3.4 Class 4, IDs 20/21/22 (read side): the boiler's own reported Day-of-week/Time, Date and
    # Year, combined into one "<Weekday>, YYYY-MM-DD HH:MM"-formatted string -- each of the three
    # underlying conversations (id=20/21/22) can succeed or fail independently, so a field this
    # sensor doesn't currently know shows as a placeholder (YYYY/MM/DD/HH/mm, or "?" for the
    # weekday) rather than the whole sensor going unknown -- see hub.cpp's
    # publish_date_time_text_(). Independent of time_id: a clock-drift/troubleshooting value, not
    # something watched day-to-day, so DIAGNOSTIC -- same reasoning as time_synchronized.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DATE_TIME: text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_OPENTHERM42_ID): cv.use_id(OpenTherm42Hub),
        **{cv.Optional(marker): schema for marker, schema in TYPES.items()},
    }
)


async def to_code(config: dict) -> None:
    hub = await cg.get_variable(config[CONF_OPENTHERM42_ID])
    for marker in TYPES:
        if (marker_config := config.get(marker)) is not None:
            var = await text_sensor.new_text_sensor(marker_config)
            cg.add(getattr(hub, f"set_{marker}_text_sensor")(var))
