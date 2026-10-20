import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_SWITCH, ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from ..climate import HaierTundra, haier_tundra_ns

CODEOWNERS = ["@fauxpark"]

HaierTundraSwitch = haier_tundra_ns.class_(
    "HaierTundraSwitch", switch.Switch, cg.Component
)
SwitchKind = haier_tundra_ns.enum("SwitchKind", is_class=True)

ICON_LEAF = "mdi:leaf"
ICON_ARROW_DOWN_RIGHT = "mdi:arrow-down-right"
ICON_ARROW_UP_RIGHT_BOLD = "mdi:arrow-up-right-bold"

CONF_HAIER_TUNDRA_ID = "haier_tundra_id"
CONF_HEALTH_MODE = "health_mode"
CONF_QUIET_MODE = "quiet_mode"
CONF_TURBO_MODE = "turbo_mode"


def _switch_schema(icon: str) -> cv.Schema:
    return switch.switch_schema(
        HaierTundraSwitch,
        icon=icon,
        default_restore_mode="RESTORE_DEFAULT_OFF",
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG,
    )


CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_HAIER_TUNDRA_ID): cv.use_id(HaierTundra),
        cv.Optional(CONF_HEALTH_MODE): _switch_schema(ICON_LEAF),
        cv.Optional(CONF_QUIET_MODE): _switch_schema(ICON_ARROW_DOWN_RIGHT),
        cv.Optional(CONF_TURBO_MODE): _switch_schema(ICON_ARROW_UP_RIGHT_BOLD),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_HAIER_TUNDRA_ID])
    switches = switch.sub_switches(config)
    if sw := await switches(
        CONF_HEALTH_MODE, hub.set_health_mode_switch, hub, SwitchKind.SWITCH_KIND_HEALTH
    ):
        await cg.register_component(sw, config[CONF_HEALTH_MODE])
    if sw := await switches(
        CONF_QUIET_MODE, hub.set_quiet_mode_switch, hub, SwitchKind.SWITCH_KIND_QUIET
    ):
        await cg.register_component(sw, config[CONF_QUIET_MODE])
    if sw := await switches(
        CONF_TURBO_MODE, hub.set_turbo_mode_switch, hub, SwitchKind.SWITCH_KIND_TURBO
    ):
        await cg.register_component(sw, config[CONF_TURBO_MODE])
