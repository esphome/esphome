import esphome.codegen as cg
from esphome.components import tcp_uart, uart
from esphome.components.const import CONF_DATA_BITS, CONF_PARITY, CONF_STOP_BITS
import esphome.config_validation as cv
from esphome.const import CONF_BAUD_RATE, CONF_ID
from esphome.core import CORE
from esphome.types import ConfigType

CODEOWNERS = ["@Bascht74"]
DEPENDENCIES = ["tcp_uart"]
MULTI_CONF = True

CONF_TCP_UART_ID = "tcp_uart_id"

modbus_tcp_ns = cg.esphome_ns.namespace("modbus_tcp")
ModbusTcp = modbus_tcp_ns.class_("ModbusTcp", uart.UARTComponent, cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ModbusTcp),
        cv.Required(CONF_TCP_UART_ID): cv.use_id(tcp_uart.TcpUart),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    parent = await cg.get_variable(config[CONF_TCP_UART_ID])
    cg.add(var.set_parent(parent))
    # The socket is not clocked. The hub reads these from its UART during
    # setup(), and every cg.add() runs before App.setup().
    parent_config = CORE.config.get_config_for_path(
        CORE.config.get_path_for_id(config[CONF_TCP_UART_ID])[:-1]
    )
    cg.add(var.set_baud_rate(parent_config[CONF_BAUD_RATE]))
    cg.add(var.set_data_bits(parent_config[CONF_DATA_BITS]))
    cg.add(var.set_stop_bits(parent_config[CONF_STOP_BITS]))
    cg.add(var.set_parity(parent_config[CONF_PARITY]))
