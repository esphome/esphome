import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
)

CODEOWNERS = ["@khenderick"]
DEPENDENCIES = ["api"]
AUTO_LOAD = ["sensor", "binary_sensor", "switch"]

CONF_PIN_IN = "pin_in"
CONF_PIN_OUT = "pin_out"
CONF_OPENTHERM_ID = "opentherm_id"

opentherm = cg.esphome_ns.namespace("opentherm")
OpenThermComponent = opentherm.class_(
    "OpenThermComponent", cg.PollingComponent
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(OpenThermComponent),
        cv.Required(CONF_PIN_IN): cv.uint8_t,
        cv.Required(CONF_PIN_OUT): cv.uint8_t,
    }
).extend(cv.polling_component_schema("5s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_pins(config[CONF_PIN_IN], config[CONF_PIN_OUT]))
