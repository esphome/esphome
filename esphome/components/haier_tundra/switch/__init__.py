import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_SWITCH, ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from ..climate import HaierTundra, haier_tundra_ns

CODEOWNERS = ["@fauxpark"]

HealthSwitch = haier_tundra_ns.class_("HealthSwitch", switch.Switch, cg.Component)
QuietSwitch = haier_tundra_ns.class_("QuietSwitch", switch.Switch, cg.Component)
TurboSwitch = haier_tundra_ns.class_("TurboSwitch", switch.Switch, cg.Component)

ICON_LEAF = "mdi:leaf"
ICON_ARROW_DOWN_RIGHT = "mdi:arrow-down-right"
ICON_ARROW_UP_RIGHT_BOLD = "mdi:arrow-up-right-bold"

CONF_HAIER_TUNDRA_ID = "haier_tundra_id"
CONF_HEALTH_MODE = "health_mode"
CONF_QUIET_MODE = "quiet_mode"
CONF_TURBO_MODE = "turbo_mode"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_HAIER_TUNDRA_ID): cv.use_id(HaierTundra),
        cv.Optional(CONF_HEALTH_MODE): switch.switch_schema(
            HealthSwitch,
            icon=ICON_LEAF,
            default_restore_mode="RESTORE_DEFAULT_OFF",
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_QUIET_MODE): switch.switch_schema(
            QuietSwitch,
            icon=ICON_ARROW_DOWN_RIGHT,
            default_restore_mode="RESTORE_DEFAULT_OFF",
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_TURBO_MODE): switch.switch_schema(
            TurboSwitch,
            icon=ICON_ARROW_UP_RIGHT_BOLD,
            default_restore_mode="RESTORE_DEFAULT_OFF",
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_HAIER_TUNDRA_ID])

    if switch_conf := config.get(CONF_HEALTH_MODE):
        var = cg.new_Pvariable(switch_conf[cv.CONF_ID])
        await switch.register_switch(var, switch_conf)
        await cg.register_component(var, switch_conf)
        await cg.register_parented(var, parent)

    if switch_conf := config.get(CONF_QUIET_MODE):
        var = cg.new_Pvariable(switch_conf[cv.CONF_ID])
        await switch.register_switch(var, switch_conf)
        await cg.register_component(var, switch_conf)
        await cg.register_parented(var, parent)

    if switch_conf := config.get(CONF_TURBO_MODE):
        var = cg.new_Pvariable(switch_conf[cv.CONF_ID])
        await switch.register_switch(var, switch_conf)
        await cg.register_component(var, switch_conf)
        await cg.register_parented(var, parent)
