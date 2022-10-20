import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import (
    CONF_ID,
)

CODEOWNERS = ["@khenderick"]
DEPENDENCIES = ["api"]
AUTO_LOAD = ["sensor", "binary_sensor", "switch"]

CONF_PIN_IN = "pin_in"
CONF_PIN_OUT = "pin_out"
CONF_DIYLESS_OPENTHERM_ID = "diyless_opentherm_id"

diyless_opentherm = cg.esphome_ns.namespace("diyless_opentherm")
DiyLessOpenThermComponent = diyless_opentherm.class_(
    "DiyLessOpenThermComponent", cg.PollingComponent
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DiyLessOpenThermComponent),
        cv.Required(CONF_PIN_IN): cv.uint8_t,
        cv.Required(CONF_PIN_OUT): cv.uint8_t,
    }
).extend(cv.polling_component_schema("5s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.initialize(config[CONF_PIN_IN], config[CONF_PIN_OUT]))
