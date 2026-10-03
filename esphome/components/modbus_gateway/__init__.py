import esphome.codegen as cg
from esphome.components import uart
from esphome.components.const import CONF_DATA_BITS, CONF_PARITY, CONF_STOP_BITS
import esphome.config_validation as cv
from esphome.const import CONF_BAUD_RATE, CONF_ID, CONF_UART_ID
from esphome.core import CORE
from esphome.types import ConfigType

CODEOWNERS = ["@Bascht74"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["modbus"]
MULTI_CONF = True

CONF_PORTS = "ports"
CONF_RESPONSE_TIMEOUT = "response_timeout"

modbus_gateway_ns = cg.esphome_ns.namespace("modbus_gateway")
ModbusGateway = modbus_gateway_ns.class_("ModbusGateway", cg.Component, uart.UARTDevice)
GatewayUart = modbus_gateway_ns.class_("GatewayUart", uart.UARTComponent)


def _port_schema(value):
    value = cv.Schema(
        {
            cv.Optional(CONF_UART_ID): cv.use_id(uart.UARTComponent),
            cv.Optional(CONF_ID): cv.declare_id(GatewayUart),
        }
    )(value)
    has_uart = CONF_UART_ID in value
    has_id = CONF_ID in value
    if has_uart == has_id:
        raise cv.Invalid("Specify uart_id or id, not both")
    return value


def _unique_ports(config: ConfigType) -> ConfigType:
    seen = [str(config[CONF_UART_ID])]
    for port in config[CONF_PORTS]:
        if CONF_UART_ID not in port:
            continue
        key = str(port[CONF_UART_ID])
        if key in seen:
            raise cv.Invalid("Each UART can be used once", path=[CONF_PORTS])
        seen.append(key)
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ModbusGateway),
            cv.Required(CONF_UART_ID): cv.use_id(uart.UARTComponent),
            cv.Optional(CONF_RESPONSE_TIMEOUT, default="500ms"): cv.All(
                cv.positive_not_null_time_period, cv.positive_time_period_milliseconds
            ),
            cv.Required(CONF_PORTS): cv.All(
                cv.ensure_list(_port_schema), cv.Length(min=1, max=4)
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _unique_ports,
)


def _final_validate(config: ConfigType) -> ConfigType:
    uart.final_validate_device_schema(
        "modbus_gateway", require_tx=True, require_rx=True
    )(config)
    for port in config[CONF_PORTS]:
        if CONF_UART_ID in port:
            uart.final_validate_device_schema(
                "modbus_gateway", require_tx=True, require_rx=True
            )(port)
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_response_timeout(config[CONF_RESPONSE_TIMEOUT]))
    ports = config[CONF_PORTS]
    cg.add(var.set_port_count(len(ports)))
    bus = CORE.config.get_config_for_path(
        CORE.config.get_path_for_id(config[CONF_UART_ID])[:-1]
    )
    for index, port in enumerate(ports):
        if CONF_UART_ID in port:
            parent = await cg.get_variable(port[CONF_UART_ID])
            cg.add(var.set_port_uart(index, parent))
            continue
        local = cg.new_Pvariable(port[CONF_ID])
        cg.add(var.set_port_local(index, local))
        cg.add(local.set_baud_rate(bus[CONF_BAUD_RATE]))
        cg.add(local.set_data_bits(bus[CONF_DATA_BITS]))
        cg.add(local.set_stop_bits(bus[CONF_STOP_BITS]))
        cg.add(local.set_parity(bus[CONF_PARITY]))
