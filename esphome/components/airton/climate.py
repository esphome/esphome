from esphome import automation
import esphome.codegen as cg
from esphome.components import climate_ir
import esphome.config_validation as cv
from esphome.types import ConfigType

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@Procsiab"]

airton_ns = cg.esphome_ns.namespace("airton")
AirtonClimate = airton_ns.class_("AirtonClimate", climate_ir.ClimateIR)

CONF_AIRTON_ID = "airton_id"
CONF_SLEEP_MODE = "sleep_mode"
CONF_VERTICAL_DIRECTION = "vertical_direction"

VerticalDirection = airton_ns.enum("VerticalDirection", is_class=True)
# In the order of the enum, so the option index is the direction value
VERTICAL_DIRECTIONS = VerticalDirection.options(
    "off",
    "swing",
    "up",
    "middle-up",
    "middle",
    "middle-down",
    "down",
    prefix="VERTICAL_DIRECTION_",
)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(AirtonClimate).extend(
    {
        cv.Optional(CONF_VERTICAL_DIRECTION, default="off"): cv.enum(
            VERTICAL_DIRECTIONS
        ),
    }
)

AIRTON_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(AirtonClimate),
    }
)

for _name, _call in (
    (
        "climate.airton.display_on",
        "set_setting(esphome::airton::AirtonSetting::AIRTON_SETTING_DISPLAY, true)",
    ),
    (
        "climate.airton.display_off",
        "set_setting(esphome::airton::AirtonSetting::AIRTON_SETTING_DISPLAY, false)",
    ),
    (
        "climate.airton.sleep_on",
        "set_setting(esphome::airton::AirtonSetting::AIRTON_SETTING_SLEEP_MODE, true)",
    ),
    (
        "climate.airton.sleep_off",
        "set_setting(esphome::airton::AirtonSetting::AIRTON_SETTING_SLEEP_MODE, false)",
    ),
):
    automation.register_apply_action(
        _name, AIRTON_ACTION_SCHEMA, automation.ApplyCall(_call)
    )


async def to_code(config: ConfigType) -> None:
    var = await climate_ir.new_climate_ir(config)
    # The default until a saved setting is restored in setup(); off is the member's initial value
    if (direction := config[CONF_VERTICAL_DIRECTION]) != "off":
        cg.add(var.set_default_vertical_direction(direction))
