from esphome import pins
import esphome.codegen as cg
from esphome.components import pn71xx
import esphome.config_validation as cv
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

AUTO_LOAD = ["pn71xx"]
CODEOWNERS = ["@kbx81", "@jesserockz"]

CONF_DWL_REQ_PIN = "dwl_req_pin"
CONF_WKUP_REQ_PIN = "wkup_req_pin"

pn7160_ns = cg.esphome_ns.namespace("pn7160")
PN7160 = pn7160_ns.class_("PN7160", pn71xx.PN71xx)

PN7160_SCHEMA = pn71xx.PN71XX_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(PN7160),
        cv.Optional(CONF_DWL_REQ_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_WKUP_REQ_PIN): pins.gpio_output_pin_schema,
    }
)

pn71xx.register_is_writing_condition("pn7160.is_writing", PN7160)


async def setup_pn7160(var: MockObj, config: ConfigType) -> None:
    await pn71xx.setup_pn71xx(var, config)

    if dwl_req_pin_config := config.get(CONF_DWL_REQ_PIN):
        pin = await cg.gpio_pin_expression(dwl_req_pin_config)
        cg.add(var.set_dwl_req_pin(pin))

    if wkup_req_pin_config := config.get(CONF_WKUP_REQ_PIN):
        pin = await cg.gpio_pin_expression(wkup_req_pin_config)
        cg.add(var.set_wkup_req_pin(pin))
