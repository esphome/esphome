from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import key_provider
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_ON_KEY, CONF_ON_TAG
from esphome.types import ConfigType

CODEOWNERS = ["@ssieb"]
DOMAIN = "wiegand"

AUTO_LOAD = ["key_provider"]

MULTI_CONF = True

wiegand_ns = cg.esphome_ns.namespace("wiegand")

Wiegand = wiegand_ns.class_("Wiegand", key_provider.KeyProvider, cg.Component)

CONF_D0 = "d0"
CONF_D1 = "d1"
CONF_ON_RAW = "on_raw"

_AUTOMATION_LISTS = (
    automation.AutomationList(
        CONF_ON_TAG,
        "add_on_tag_automation",
        "WIEGAND_ON_TAG_COUNT",
        [(cg.std_string, "x")],
    ),
    automation.AutomationList(
        CONF_ON_RAW,
        "add_on_raw_automation",
        "WIEGAND_ON_RAW_COUNT",
        [(cg.uint8, "bits"), (cg.uint64, "value")],
    ),
    automation.AutomationList(
        CONF_ON_KEY, "add_on_key_automation", "WIEGAND_ON_KEY_COUNT", [(cg.uint8, "x")]
    ),
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Wiegand),
        cv.Required(CONF_D0): pins.internal_gpio_input_pin_schema,
        cv.Required(CONF_D1): pins.internal_gpio_input_pin_schema,
        cv.Optional(CONF_ON_TAG): automation.validate_automation(),
        cv.Optional(CONF_ON_RAW): automation.validate_automation(),
        cv.Optional(CONF_ON_KEY): automation.validate_automation(),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cg.add_pin(var.set_d0_pin, config[CONF_D0])
    await cg.add_pin(var.set_d1_pin, config[CONF_D1])

    await automation.build_automation_lists(var, config, _AUTOMATION_LISTS)
