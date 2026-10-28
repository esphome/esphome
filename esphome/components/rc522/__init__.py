from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ON_TAG, CONF_ON_TAG_REMOVED, CONF_RESET_PIN
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

CODEOWNERS = ["@glmnet"]
AUTO_LOAD = ["binary_sensor"]
DOMAIN = "rc522"

CONF_RC522_ID = "rc522_id"

rc522_ns = cg.esphome_ns.namespace("rc522")
RC522 = rc522_ns.class_("RC522", cg.PollingComponent, i2c.I2CDevice)

_AUTOMATION_LISTS = (
    automation.AutomationList(
        CONF_ON_TAG, "RC522_ON_TAG_COUNT", [(cg.std_string, "x")]
    ),
    automation.AutomationList(
        CONF_ON_TAG_REMOVED, "RC522_ON_TAG_REMOVED_COUNT", [(cg.std_string, "x")]
    ),
)

RC522_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(RC522),
        cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_ON_TAG): automation.validate_automation(),
        cv.Optional(CONF_ON_TAG_REMOVED): automation.validate_automation(),
    }
).extend(cv.polling_component_schema("1s"))


async def setup_rc522(var: MockObj, config: ConfigType) -> None:
    await cg.register_component(var, config)

    await cg.add_pin(var.set_reset_pin, config.get(CONF_RESET_PIN))

    await automation.build_automation_lists(var, config, _AUTOMATION_LISTS)
