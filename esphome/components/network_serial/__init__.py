"""RFC 2217 Network Serial component for ESPHome.

Supports two modes:
  - client: Connect to a remote RFC 2217 server
  - server: Expose a local UART as an RFC 2217 server
"""

import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODE, CONF_PORT, CONF_UART_ID

DEPENDENCIES = ["uart"]
MULTI_CONF = True

# Constants
CONF_HOST = "host"
CONF_BAUDRATE = "baudrate"
CONF_DATA_BITS = "data_bits"
CONF_PARITY = "parity"
CONF_STOP_BITS = "stop_bits"
CONF_FLOW_CONTROL = "flow_control"
CONF_DTR = "dtr"
CONF_RTS = "rts"
CONF_DEVICE_NODE = "device_node"

# Namespace
network_serial_ns = cg.esphome_ns.namespace("network_serial")
NetworkSerialClient = network_serial_ns.class_("NetworkSerialClient", cg.Component)
NetworkSerialServer = network_serial_ns.class_("NetworkSerialServer", cg.Component)

# Enums
ParityMode = network_serial_ns.enum("ParityMode")
PARITY_OPTIONS = {
    "NONE": ParityMode.PARITY_NONE,
    "EVEN": ParityMode.PARITY_EVEN,
    "ODD": ParityMode.PARITY_ODD,
    "MARK": ParityMode.PARITY_MARK,
    "SPACE": ParityMode.PARITY_SPACE,
}

FlowControl = network_serial_ns.enum("FlowControl")
FLOW_CONTROL_OPTIONS = {
    "NONE": FlowControl.FLOW_NONE,
    "SOFTWARE": FlowControl.FLOW_XONXOFF,
    "XONXOFF": FlowControl.FLOW_XONXOFF,
    "HARDWARE": FlowControl.FLOW_HARDWARE,
    "RTS_CTS": FlowControl.FLOW_HARDWARE,
}

# Default values
DEFAULT_PORT = 2217
DEFAULT_BAUDRATE = 9600
DEFAULT_DATA_BITS = 8
DEFAULT_PARITY = "NONE"
DEFAULT_STOP_BITS = 1
DEFAULT_FLOW_CONTROL = "NONE"

MODE_CLIENT = "client"
MODE_SERVER = "server"

# Client mode schema (connects to remote RFC 2217 server)
CLIENT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(NetworkSerialClient),
        cv.Optional(CONF_MODE, default=MODE_CLIENT): cv.one_of(
            MODE_CLIENT, MODE_SERVER, lower=True
        ),
        cv.Required(CONF_HOST): cv.string,
        cv.Optional(CONF_PORT, default=DEFAULT_PORT): cv.port,
        cv.Optional(CONF_BAUDRATE, default=DEFAULT_BAUDRATE): cv.int_range(
            min=300, max=4000000
        ),
        cv.Optional(CONF_DATA_BITS, default=DEFAULT_DATA_BITS): cv.int_range(
            min=5, max=8
        ),
        cv.Optional(CONF_PARITY, default=DEFAULT_PARITY): cv.enum(
            PARITY_OPTIONS, upper=True
        ),
        cv.Optional(CONF_STOP_BITS, default=DEFAULT_STOP_BITS): cv.one_of(
            1, 2, int=True
        ),
        cv.Optional(CONF_FLOW_CONTROL, default=DEFAULT_FLOW_CONTROL): cv.enum(
            FLOW_CONTROL_OPTIONS, upper=True
        ),
        cv.Optional(CONF_DTR, default=False): cv.boolean,
        cv.Optional(CONF_RTS, default=False): cv.boolean,
        cv.Optional(CONF_DEVICE_NODE): cv.string,
    }
).extend(cv.COMPONENT_SCHEMA)

# Server mode schema (exposes local UART over RFC 2217)
SERVER_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(NetworkSerialServer),
        cv.Optional(CONF_MODE, default=MODE_CLIENT): cv.one_of(
            MODE_CLIENT, MODE_SERVER, lower=True
        ),
        cv.Required(CONF_UART_ID): cv.use_id(uart.UARTComponent),
        cv.Optional(CONF_PORT, default=DEFAULT_PORT): cv.port,
    }
).extend(cv.COMPONENT_SCHEMA)


def _validate_config(config):
    """Route to the correct schema based on mode."""
    mode = config.get(CONF_MODE, MODE_CLIENT)
    if mode == MODE_SERVER:
        return SERVER_SCHEMA(config)
    return CLIENT_SCHEMA(config)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_MODE, default=MODE_CLIENT): cv.one_of(
                MODE_CLIENT, MODE_SERVER, lower=True
            ),
        },
        extra=cv.ALLOW_EXTRA,
    ),
    _validate_config,
)


async def to_code(config):
    """Generate code for network serial component."""
    mode = config.get(CONF_MODE, "client")

    # Add network dependency
    cg.add_define("USE_NETWORK")

    if mode == "server":
        await _to_code_server(config)
    else:
        await _to_code_client(config)


async def _to_code_client(config):
    """Generate code for client mode."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_host(config[CONF_HOST]))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_baudrate(config[CONF_BAUDRATE]))
    cg.add(var.set_data_bits(config[CONF_DATA_BITS]))
    cg.add(var.set_parity(config[CONF_PARITY]))
    cg.add(var.set_stop_bits(config[CONF_STOP_BITS]))
    cg.add(var.set_flow_control(config[CONF_FLOW_CONTROL]))
    cg.add(var.set_dtr(config[CONF_DTR]))
    cg.add(var.set_rts(config[CONF_RTS]))

    if CONF_DEVICE_NODE in config:
        cg.add(var.set_device_node(config[CONF_DEVICE_NODE]))


async def _to_code_server(config):
    """Generate code for server mode."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    uart_component = await cg.get_variable(config[CONF_UART_ID])
    cg.add(var.set_uart(uart_component))
    cg.add(var.set_port(config[CONF_PORT]))
