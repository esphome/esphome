import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from .. import CONF_LOKI_ID, Loki, loki_ns

CODEOWNERS = ["@jzucker2"]
EnabledSwitch = loki_ns.class_("EnabledSwitch", switch.Switch)

# Haier switches
CONF_ENABLED = "enabled"

# Additional icons
ICON_LED_ON = "mdi:math-log"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LOKI_ID): cv.use_id(Loki),
        cv.Optional(CONF_ENABLED): switch.switch_schema(
            EnabledSwitch,
            icon=ICON_LED_ON,
            entity_category=ENTITY_CATEGORY_CONFIG,
            default_restore_mode="ENABLED",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_LOKI_ID])

    for switch_type in [CONF_ENABLED]:
        if conf := config.get(switch_type):
            sw_var = await switch.new_switch(conf)
            await cg.register_parented(sw_var, parent)
            cg.add(getattr(parent, f"set_{switch_type}_switch")(sw_var))
