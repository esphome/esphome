import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG, CONF_BAUD_RATE
from .. import CONF_LD2410_ID, LD2410Component, ld2410_ns

LD2410Select = ld2410_ns.class_("LD2410Select", select.Select)

CONF_DISTANCE_RESOLUTION = "distance_resolution"
CONF_LIGHT_FUNCTION = "light_function"
CONF_OUT_PIN_LEVEL = "out_pin_level"


def validate(config):
    has_some_light_configs = (
        CONF_LIGHT_FUNCTION in config or CONF_OUT_PIN_LEVEL in config
    )
    has_all_light_configs = (
        CONF_LIGHT_FUNCTION in config and CONF_OUT_PIN_LEVEL in config
    )
    if has_some_light_configs and not has_all_light_configs:
        raise cv.Invalid(
            f"{CONF_LIGHT_FUNCTION} and {CONF_OUT_PIN_LEVEL} are all must be set"
        )
    return config


CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2410_ID): cv.use_id(LD2410Component),
    cv.Optional(CONF_DISTANCE_RESOLUTION): select.select_schema(
        LD2410Select,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:social-distance-2-meters",
    ),
    cv.Optional(CONF_LIGHT_FUNCTION): select.select_schema(
        LD2410Select,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:sun-wireless-outline",
    ),
    cv.Optional(CONF_OUT_PIN_LEVEL): select.select_schema(
        LD2410Select,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:pin",
    ),
    cv.Optional(CONF_BAUD_RATE): select.select_schema(
        LD2410Select,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:speedometer",
    ),
}

CONFIG_SCHEMA = cv.All(CONFIG_SCHEMA, validate)


async def to_code(config):
    ld2410_component = await cg.get_variable(config[CONF_LD2410_ID])
    if CONF_DISTANCE_RESOLUTION in config:
        s = await select.new_select(
            config[CONF_DISTANCE_RESOLUTION], options=["0.2m", "0.75m"]
        )
        cg.add(ld2410_component.set_distance_resolution_select(s))
    if CONF_OUT_PIN_LEVEL in config:
        s = await select.new_select(config[CONF_OUT_PIN_LEVEL], options=["low", "high"])
        cg.add(ld2410_component.set_out_pin_level_select(s))
    if CONF_LIGHT_FUNCTION in config:
        s = await select.new_select(
            config[CONF_LIGHT_FUNCTION], options=["off", "below", "above"]
        )
        cg.add(ld2410_component.set_light_function_select(s))
    if CONF_BAUD_RATE in config:
        s = await select.new_select(
            config[CONF_BAUD_RATE],
            options=[
                "9600",
                "19200",
                "38400",
                "57600",
                "115200",
                "230400",
                "256000",
                "460800",
            ],
        )
        cg.add(ld2410_component.set_baud_rate_select(s))
