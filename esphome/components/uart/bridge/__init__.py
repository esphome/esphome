import esphome.codegen as cg
from esphome.components import uart
from esphome.components.bridge import DOMAIN as BRIDGE_DOMAIN
import esphome.config_validation as cv
from esphome.const import CONF_DEBUG, CONF_DUMMY_RECEIVER, CONF_ID, CONF_UART_ID
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import uart_ns

CODEOWNERS = ["@Bascht74"]
DEPENDENCIES = ["uart"]

CONF_PEER_ID = "peer_id"

UARTBridge = uart_ns.class_("UARTBridge", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(UARTBridge),
        cv.Required(CONF_UART_ID): cv.use_id(uart.UARTComponent),
        cv.Required(CONF_PEER_ID): cv.use_id(uart.UARTComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


def _subtree_references_uart(node: object, uart_id: str) -> bool:
    if isinstance(node, dict):
        return any(
            (key == CONF_UART_ID and str(value) == uart_id)
            or _subtree_references_uart(value, uart_id)
            for key, value in node.items()
        )
    if isinstance(node, list):
        return any(_subtree_references_uart(item, uart_id) for item in node)
    return False


def _reject_dummy_receiver(uart_conf: ConfigType) -> ConfigType:
    debug = uart_conf.get(CONF_DEBUG)
    if isinstance(debug, dict) and debug.get(CONF_DUMMY_RECEIVER):
        raise cv.Invalid(
            "dummy_receiver reads this UART and drops the bytes the bridge should forward.",
            [CONF_DEBUG, CONF_DUMMY_RECEIVER],
        )
    return uart_conf


def _final_validate(config: ConfigType) -> ConfigType:
    # Same seen-set as the CDC-ACM bridge, so the two platforms cannot share an interface.
    full_config = fv.full_config.get()
    ends = (
        (CONF_UART_ID, str(config[CONF_UART_ID])),
        (CONF_PEER_ID, str(config[CONF_PEER_ID])),
    )
    if ends[0][1] == ends[1][1]:
        raise cv.Invalid("The two ends are the same UART.", [CONF_PEER_ID])

    data = full_config.data.setdefault(BRIDGE_DOMAIN, {})
    used = data.setdefault(CONF_UART_ID, set())
    for key, uart_id in ends:
        if uart_id in used:
            raise cv.Invalid(
                f"The UART '{uart_id}' is already bridged by another 'bridge' instance.",
                [key],
            )
        used.add(uart_id)
        for domain, domain_conf in full_config.items():
            if domain == BRIDGE_DOMAIN:
                continue
            if _subtree_references_uart(domain_conf, uart_id):
                raise cv.Invalid(
                    f"The UART '{uart_id}' is also used by '{domain}'. "
                    "A bridge requires exclusive use of that UART.",
                    [key],
                )
    fv.id_declaration_match_schema(_reject_dummy_receiver)(config[CONF_UART_ID])
    fv.id_declaration_match_schema(_reject_dummy_receiver)(config[CONF_PEER_ID])
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_a(await cg.get_variable(config[CONF_UART_ID])))
    cg.add(var.set_b(await cg.get_variable(config[CONF_PEER_ID])))
