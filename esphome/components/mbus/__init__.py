import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart

CODEOWNERS = ["@MarkusSchneider"]
DEPENDENCIES = ["uart"]

mbus_ns = cg.esphome_ns.namespace("mbus")
MBus = mbus_ns.class_("MBus", cg.Component)
MULTI_CONF = False

CONF_MBUS_ID = "mbus_id"
CONF_SECONDARY_ADDRESS = "secondary_address"
CONF_INTERVAL = "interval"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(CONF_MBUS_ID): cv.declare_id(MBus),
            cv.Optional(CONF_SECONDARY_ADDRESS, default=0): cv.hex_uint64_t,
            cv.Optional(CONF_INTERVAL, default="1min"): cv.positive_time_period_seconds,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    cg.add_global(mbus_ns.using)
    var = cg.new_Pvariable(config[CONF_MBUS_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_secondary_address(config[CONF_SECONDARY_ADDRESS]))
    cg.add(var.set_interval(config[CONF_INTERVAL]))
