import logging

from esphome import automation
import esphome.codegen as cg
from esphome.components import uart
from esphome.components.const import CONF_ENABLED
import esphome.config_validation as cv
from esphome.const import CONF_ID

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@functionpointer"]
DEPENDENCIES = ["uart"]
MULTI_CONF = True

CONF_PYLONTECH_ID = "pylontech_id"
CONF_BATTERY = "battery"

pylontech_ns = cg.esphome_ns.namespace("pylontech")
PylontechComponent = pylontech_ns.class_(
    "PylontechComponent", cg.PollingComponent, uart.UARTDevice
)
PylontechBattery = pylontech_ns.class_("PylontechBattery")
SetCellPollingAction = pylontech_ns.class_("SetCellPollingAction", automation.Action)

CV_NUM_BATTERIES = cv.int_range(1, 16)

PYLONTECH_COMPONENT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_PYLONTECH_ID): cv.use_id(PylontechComponent),
        cv.Required(CONF_BATTERY): CV_NUM_BATTERIES,
    }
)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PylontechComponent),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


@automation.register_action(
    "pylontech.set_cell_polling",
    SetCellPollingAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(PylontechComponent),
            cv.Required(CONF_ENABLED): cv.templatable(cv.boolean),
        }
    ),
    synchronous=True,
)
async def set_cell_polling_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    template_ = await cg.templatable(config[CONF_ENABLED], args, cg.bool_)
    cg.add(var.set_enable(template_))
    return var


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
