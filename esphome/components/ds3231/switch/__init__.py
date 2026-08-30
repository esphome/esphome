import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from .. import (
    CONF_DS3231_ID,
    ICON_SINE_WAVE,
    USE_DS3231_32KHZ_OUTPUT,
    USE_DS3231_ALARM,
    DS3231Component,
    ds3231_ns,
)

DEPENDENCIES = ["ds3231"]

CONF_ENABLE_32KHZ_OUTPUT = "enable_32khz_output"
CONF_ALARM_1 = "alarm_1"
CONF_ALARM_2 = "alarm_2"

DS3231Enable32kHzSwitch = ds3231_ns.class_(
    "DS3231Enable32kHzSwitch", switch.Switch, cg.Parented.template(DS3231Component)
)
DS3231AlarmSwitch = ds3231_ns.class_(
    "DS3231AlarmSwitch", switch.Switch, cg.Parented.template(DS3231Component)
)

_ALARM_SWITCH_SCHEMA = switch.switch_schema(
    DS3231AlarmSwitch,
    entity_category=ENTITY_CATEGORY_CONFIG,
    icon="mdi:alarm",
    default_restore_mode="DISABLED",
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DS3231_ID): cv.use_id(DS3231Component),
        cv.Optional(CONF_ENABLE_32KHZ_OUTPUT): switch.switch_schema(
            DS3231Enable32kHzSwitch,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SINE_WAVE,
            default_restore_mode="DISABLED",
        ),
        cv.Optional(CONF_ALARM_1): _ALARM_SWITCH_SCHEMA,
        cv.Optional(CONF_ALARM_2): _ALARM_SWITCH_SCHEMA,
    }
).add_extra(
    cv.has_at_least_one_key(CONF_ENABLE_32KHZ_OUTPUT, CONF_ALARM_1, CONF_ALARM_2)
)


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_DS3231_ID])

    if (conf := config.get(CONF_ENABLE_32KHZ_OUTPUT)) is not None:
        cg.add_define(USE_DS3231_32KHZ_OUTPUT)
        var = await switch.new_switch(conf)
        await cg.register_parented(var, config[CONF_DS3231_ID])

    for key, alarm, setter in (
        (CONF_ALARM_1, 1, "set_alarm_1_switch"),
        (CONF_ALARM_2, 2, "set_alarm_2_switch"),
    ):
        if (conf := config.get(key)) is not None:
            cg.add_define(USE_DS3231_ALARM)
            var = await switch.new_switch(conf)
            await cg.register_parented(var, config[CONF_DS3231_ID])
            cg.add(var.set_alarm(alarm))
            cg.add(getattr(parent, setter)(var))
