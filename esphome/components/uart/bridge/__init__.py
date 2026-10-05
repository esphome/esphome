import esphome.codegen as cg
from esphome.components import uart
from esphome.components.bridge import claim_exclusive
import esphome.config_validation as cv
from esphome.const import CONF_DEBUG, CONF_DUMMY_RECEIVER, CONF_ID, CONF_UART_ID
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import uart_ns

CODEOWNERS = ["@Bascht74"]
DEPENDENCIES = ["uart"]

CONF_PEER_ID = "peer_id"

UARTBridge = uart_ns.class_("UARTBridge", cg.Component)

# Hardware lines: clocked, and bytes show up as they arrive or in reported batches.
_WIRE_UART_CLASSES = (*uart.NATIVE_UART_CLASSES, str(uart.HostUartComponent))

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(UARTBridge),
        cv.Required(CONF_UART_ID): cv.use_id(uart.UARTComponent),
        cv.Required(CONF_PEER_ID): cv.use_id(uart.UARTComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


def _reject_dummy_receiver(uart_conf: ConfigType) -> ConfigType:
    debug = uart_conf.get(CONF_DEBUG)
    if isinstance(debug, dict) and debug.get(CONF_DUMMY_RECEIVER):
        raise cv.Invalid(
            "dummy_receiver reads this UART and drops the bytes the bridge should forward.",
            [CONF_DEBUG, CONF_DUMMY_RECEIVER],
        )
    return uart_conf


def _final_validate(config: ConfigType) -> ConfigType:
    if str(config[CONF_UART_ID]) == str(config[CONF_PEER_ID]):
        raise cv.Invalid("The two ends are the same UART.", [CONF_PEER_ID])
    for key in (CONF_UART_ID, CONF_PEER_ID):
        # Both keys hold UARTs, so a UART bridged as uart_id here and peer_id elsewhere is caught.
        claim_exclusive(config, key, "UART", seen_key=CONF_UART_ID)
        fv.id_declaration_match_schema(_reject_dummy_receiver)(config[key])
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    a_id, a = await cg.get_variable_with_full_id(config[CONF_UART_ID])
    b_id, b = await cg.get_variable_with_full_id(config[CONF_PEER_ID])
    var = cg.new_Pvariable(config[CONF_ID], a, b)
    await cg.register_component(var, config)
    # A virtual end pushes whole blocks and takes whole writes, so nothing polls it.
    if a_id.type.inherits_from(uart.VirtualUARTComponent):
        cg.add(var.set_virtual_a(a))
    elif str(a_id.type) in _WIRE_UART_CLASSES:
        cg.add(var.set_wire_a())
    if b_id.type.inherits_from(uart.VirtualUARTComponent):
        cg.add(var.set_virtual_b(b))
    elif str(b_id.type) in _WIRE_UART_CLASSES:
        cg.add(var.set_wire_b())
