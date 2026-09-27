import esphome.codegen as cg
from esphome.components import ota
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

CODEOWNERS = ["@esphome/tests"]
DEPENDENCIES = ["ota"]

CONF_READY_AFTER = "ready_after"

ota_prepare_test_component_ns = cg.esphome_ns.namespace("ota_prepare_test_component")
OTAPrepareTestComponent = ota_prepare_test_component_ns.class_(
    "OTAPrepareTestComponent", cg.Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(OTAPrepareTestComponent),
        cv.Optional(
            CONF_READY_AFTER, default="300ms"
        ): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_ready_after(config[CONF_READY_AFTER]))
    await ota.register_ota_prepare_listener(var)
