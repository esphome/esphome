import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]

mbus_ns = cg.esphome_ns.namespace("mbus")
MBus = mbus_ns.class_("MBus", cg.Component)
MULTI_CONF = True

CONF_MBUS_SECONDARY_ADDRESS = "secondary_address"
CONF_MBUS_DELAY = "delay"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MBus),
            cv.Optional(CONF_MBUS_SECONDARY_ADDRESS, default=0): cv.hex_uint64_t,
            cv.Optional(
                CONF_MBUS_DELAY, default="1min"
            ): cv.positive_time_period_seconds,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    cg.add_global(mbus_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_secondary_address(config[CONF_MBUS_SECONDARY_ADDRESS]))
    cg.add(var.set_delay(config[CONF_MBUS_DELAY]))
