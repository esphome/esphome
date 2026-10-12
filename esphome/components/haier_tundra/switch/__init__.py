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

    for switch_type in [CONF_HEALTH_MODE, CONF_QUIET_MODE, CONF_TURBO_MODE]:
        if conf := config.get(switch_type):
            sw = await switch.new_switch(conf)
            await cg.register_component(sw, conf)
            await cg.register_parented(sw, parent)
            cg.add(getattr(parent, f"set_{switch_type}_switch")(sw))
