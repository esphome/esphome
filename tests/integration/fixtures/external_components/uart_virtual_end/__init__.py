import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_BAUD_RATE, CONF_ID
from esphome.types import ConfigType

CODEOWNERS = ["@esphome/tests"]
DEPENDENCIES = ["uart"]
MULTI_CONF = True

CONF_PEER_ID = "peer_id"

uart_virtual_end_ns = cg.esphome_ns.namespace("uart_virtual_end")
UartVirtualEnd = uart_virtual_end_ns.class_(
    "UartVirtualEnd", uart.VirtualUARTComponent, cg.Component
)

# One end of a wireless null-modem pair: what one end writes, its peer receives.
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(UartVirtualEnd),
        cv.Required(CONF_PEER_ID): cv.use_id(UartVirtualEnd),
        cv.Optional(CONF_BAUD_RATE, default=9600): cv.int_range(min=1),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    uart.require_virtual_uart()
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_label(str(config[CONF_ID])))
    cg.add(var.set_baud_rate(config[CONF_BAUD_RATE]))
    cg.add(var.set_peer(await cg.get_variable(config[CONF_PEER_ID])))
