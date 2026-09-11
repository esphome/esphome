import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from .. import OpenTherm42Hub
from ..const import (
    CONF_CONFIGURATION_INFORMATION_BRAND,
    CONF_CONFIGURATION_INFORMATION_BRAND_SERIAL_NUMBER,
    CONF_CONFIGURATION_INFORMATION_BRAND_VERSION,
    CONF_CONTROL_AND_STATUS_INFORMATION_SOLAR_STORAGE_MODE_AND_STATUS_SOLAR_MODE,
    CONF_CONTROL_AND_STATUS_INFORMATION_SOLAR_STORAGE_MODE_AND_STATUS_SOLAR_STATUS,
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_OPERATING_MODE_DHW,
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_OPERATING_MODE_HEATING_HC1,
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_OPERATING_MODE_HEATING_HC2,
    CONF_OPENTHERM42_ID,
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
