import esphome.codegen as cg
from esphome.components import uart
from esphome.components.const import (
    CONF_DATA_BITS,
    CONF_PARITY,
    CONF_ROLE,
    CONF_STOP_BITS,
)
import esphome.config_validation as cv
from esphome.const import CONF_BAUD_RATE, CONF_DISABLED, CONF_ID, CONF_UART_ID
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@Bascht74"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["modbus"]
MULTI_CONF = True

CONF_PORTS = "ports"
CONF_RESPONSE_TIMEOUT = "response_timeout"
CONF_TCP_UART_ID = "tcp_uart_id"
CONF_CACHE_TIME = "cache_time"
CONF_CACHE = "cache"
CONF_CACHE_ENTRIES = "cache_entries"

modbus_gateway_ns = cg.esphome_ns.namespace("modbus_gateway")
ModbusGateway = modbus_gateway_ns.class_("ModbusGateway", cg.Component, uart.UARTDevice)
GatewayUart = modbus_gateway_ns.class_("GatewayUart", uart.UARTComponent)


def _port_schema(value):
    value = cv.Schema(
        {
            cv.Optional(CONF_UART_ID): cv.use_id(uart.UARTComponent),
            cv.Optional(CONF_ID): cv.declare_id(GatewayUart),
            cv.Optional(CONF_CACHE, default=False): cv.boolean,
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
            cv.Optional(
                CONF_CACHE_TIME, default="500ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_CACHE_ENTRIES, default=0): cv.int_range(min=0, max=512),
            cv.Required(CONF_PORTS): cv.All(
                cv.ensure_list(_port_schema), cv.Length(min=1, max=4)
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _unique_ports,
)


def _same_id(left, right) -> bool:
    return str(left) == str(right) and str(left) != ""


def _reject_port_direction(config: ConfigType) -> ConfigType:
    """Ports are masters. A server hub writes replies, and a modbus_tcp link
    yields replies on read. Neither can be the source of a request.
    """
    full = fv.full_config.get()
    hubs = []
    links = []
    if full is not None:
        hubs = full.get("modbus") or []
        links.extend(item for item in full.get("modbus_tcp") or [] if CONF_TCP_UART_ID in item)
    for index, port in enumerate(config[CONF_PORTS]):
        if CONF_ID in port:
            local = port[CONF_ID]
            for hub in hubs:
                if not _same_id(hub.get(CONF_UART_ID, ""), local):
                    continue
                if hub.get(CONF_ROLE, "client") != "server":
                    continue
                raise cv.Invalid(
                    "A modbus hub with role: server writes a response. "
                    "This port sends that write to the bus as a request. Use role: client",
                    path=[CONF_PORTS, index],
                )
        port_uart = port.get(CONF_UART_ID)
        if port_uart is None:
            continue
        for link in links:
            if not _same_id(link.get(CONF_ID, ""), port_uart):
                continue
            raise cv.Invalid(
                "A modbus_tcp link delivers responses on read and sends requests on write. "
                "A gateway port has to read requests from a master",
                path=[CONF_PORTS, index],
            )
    return config


def _final_validate(config: ConfigType) -> ConfigType:
    uart.final_validate_device_schema(
        "modbus_gateway", require_tx=True, require_rx=True
    )(config)
    for port in config[CONF_PORTS]:
        if CONF_UART_ID in port:
            uart.final_validate_device_schema(
                "modbus_gateway", require_tx=True, require_rx=True
            )(port)
    full = fv.full_config.get()
    psram = full["psram"] if full is not None and "psram" in full else None
    psram_on = isinstance(psram, dict) and not psram.get(CONF_DISABLED, False)
    limit = 32 if CORE.is_esp8266 else (512 if psram_on else 64)
    if config[CONF_CACHE_ENTRIES] > limit:
        raise cv.Invalid(
            f"cache_entries must be at most {limit} on this device",
            path=[CONF_CACHE_ENTRIES],
        )
    return _reject_port_direction(config)


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_response_timeout(config[CONF_RESPONSE_TIMEOUT]))
    cg.add(var.set_cache_time(config[CONF_CACHE_TIME]))
    cg.add(var.set_cache_entries(config[CONF_CACHE_ENTRIES]))
    ports = config[CONF_PORTS]
    cg.add(var.set_port_count(len(ports)))
    bus = CORE.config.get_config_for_path(
        CORE.config.get_path_for_id(config[CONF_UART_ID])[:-1]
    )
    for index, port in enumerate(ports):
        cg.add(var.set_port_cache(index, port[CONF_CACHE]))
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
