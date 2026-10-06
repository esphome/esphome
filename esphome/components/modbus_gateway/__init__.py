import esphome.codegen as cg
from esphome.components import uart
from esphome.components.const import CONF_DATA_BITS, CONF_PARITY, CONF_STOP_BITS
import esphome.config_validation as cv
from esphome.const import CONF_BAUD_RATE, CONF_ID, CONF_UART_ID
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@Bascht74"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["modbus"]
MULTI_CONF = True

DOMAIN = "modbus_gateway"
CONF_PORTS = "ports"
CONF_RESPONSE_TIMEOUT = "response_timeout"

modbus_gateway_ns = cg.esphome_ns.namespace("modbus_gateway")
ModbusGateway = modbus_gateway_ns.class_("ModbusGateway", cg.Component, uart.UARTDevice)
GatewayUart = modbus_gateway_ns.class_("GatewayUart", uart.VirtualUARTComponent)


def _port_schema(value: ConfigType) -> ConfigType:
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


def _references(node: object, uart_id: str) -> bool:
    if isinstance(node, dict):
        return any(
            (key == CONF_UART_ID and str(value) == uart_id)
            or _references(value, uart_id)
            for key, value in node.items()
        )
    if isinstance(node, list):
        return any(_references(item, uart_id) for item in node)
    return False


def _require_exclusive(config: ConfigType) -> None:
    """The gateway reads the bus and every port UART. A second reader takes its bytes.

    Bare `id:` references (a uart.write action, a lambda) are not seen.
    """
    if CORE.testing_mode:
        # Grouped component tests put many components on one uart_bus.
        return
    owned = [([CONF_UART_ID], config[CONF_UART_ID])]
    owned += [
        ([CONF_PORTS, index, CONF_UART_ID], port[CONF_UART_ID])
        for index, port in enumerate(config[CONF_PORTS])
        if CONF_UART_ID in port
    ]
    full = fv.full_config.get()
    for path, uart_id in owned:
        for domain, domain_conf in full.items():
            if domain == DOMAIN:
                domain_conf = [item for item in domain_conf if item is not config]
            if _references(domain_conf, str(uart_id)):
                raise cv.Invalid(
                    f"The UART '{uart_id}' is also used by '{domain}'. "
                    "The gateway needs it for itself",
                    path=path,
                )


def _require_bus_framing(config: ConfigType) -> None:
    if not any(CONF_ID in port for port in config[CONF_PORTS]):
        return
    full = fv.full_config.get()
    bus = full.get_config_for_path(full.get_path_for_id(config[CONF_UART_ID])[:-1])
    if any(
        key not in bus
        for key in (CONF_BAUD_RATE, CONF_DATA_BITS, CONF_PARITY, CONF_STOP_BITS)
    ):
        raise cv.Invalid(
            "A port with id copies baud_rate, data_bits, parity and stop_bits from the bus. "
            f"'{config[CONF_UART_ID]}' does not set them. Use a uart bus",
            path=[CONF_UART_ID],
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
    _require_bus_framing(config)
    _require_exclusive(config)
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
        uart.require_virtual_uart()
        local = cg.new_Pvariable(port[CONF_ID])
        cg.add(var.set_port_local(index, local))
        cg.add(local.set_baud_rate(bus[CONF_BAUD_RATE]))
        cg.add(local.set_data_bits(bus[CONF_DATA_BITS]))
        cg.add(local.set_stop_bits(bus[CONF_STOP_BITS]))
        # The C++ default is no parity.
        if (parity := bus[CONF_PARITY]) != "NONE":
            cg.add(local.set_parity(parity))
