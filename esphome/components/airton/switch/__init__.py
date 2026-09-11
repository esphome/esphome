import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_DISPLAY, ENTITY_CATEGORY_CONFIG

from ..climate import CONF_AIRTON_ID, CONF_SLEEP_MODE, AirtonClimate, airton_ns

CODEOWNERS = ["@procsiab"]

SleepSwitch = airton_ns.class_("SleepSwitch", switch.Switch)
DisplaySwitch = airton_ns.class_("DisplaySwitch", switch.Switch)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AIRTON_ID): cv.use_id(AirtonClimate),
        cv.Optional(CONF_DISPLAY): switch.switch_schema(
            DisplaySwitch,
            entity_category=ENTITY_CATEGORY_CONFIG,
            default_restore_mode="DISABLED",
        ),
        cv.Optional(CONF_SLEEP_MODE): switch.switch_schema(
            SleepSwitch,
            entity_category=ENTITY_CATEGORY_CONFIG,
            default_restore_mode="DISABLED",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_AIRTON_ID])

    for switch_type in [CONF_DISPLAY, CONF_SLEEP_MODE]:
        if conf := config.get(switch_type):
            sw_var = await switch.new_switch(conf)
            await cg.register_parented(sw_var, parent)
            cg.add(getattr(parent, f"set_{switch_type}_switch")(sw_var))
