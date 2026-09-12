from esphome import automation
import esphome.codegen as cg
from esphome.components import esp32, uart
from esphome.components.cdc_acm_uart.bridge import CDCACMUARTBridge
from esphome.components.const import CONF_BRIDGE_ID
from esphome.components.esp32 import VARIANT_ESP32P4, VARIANT_ESP32S2, VARIANT_ESP32S3
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_UART_ID
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@kbx81"]
DEPENDENCIES = ["bridge", "uart"]
MULTI_CONF = True

CONF_INITIAL_ROUTE = "initial_route"
ROUTE_BRIDGE = "bridge"
ROUTE_LOCAL = "local"

uart_mux_ns = cg.esphome_ns.namespace("uart_mux")
UARTMux = uart_mux_ns.class_("UARTMux", uart.UARTComponent, cg.Component)
SelectLocalAction = uart_mux_ns.class_("SelectLocalAction", automation.Action)
SelectBridgeAction = uart_mux_ns.class_("SelectBridgeAction", automation.Action)
IsLocalCondition = uart_mux_ns.class_("IsLocalCondition", automation.Condition)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(UARTMux),
            cv.Required(CONF_UART_ID): cv.use_id(uart.IDFUARTComponent),
            cv.Required(CONF_BRIDGE_ID): cv.use_id(CDCACMUARTBridge),
            cv.Optional(CONF_INITIAL_ROUTE, default=ROUTE_BRIDGE): cv.one_of(
                ROUTE_BRIDGE, ROUTE_LOCAL, lower=True
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    esp32.only_on_variant(
        supported=[VARIANT_ESP32P4, VARIANT_ESP32S2, VARIANT_ESP32S3],
    ),
)


def _final_validate(config: ConfigType) -> ConfigType:
    # The mux hands the bridge's own UART back and forth; any other UART is a mistake.
    bridge_id = str(config[CONF_BRIDGE_ID])
    for bridge_conf in fv.full_config.get().get("bridge", []):
        if str(bridge_conf[CONF_ID]) != bridge_id:
            continue
        if str(bridge_conf[CONF_UART_ID]) != str(config[CONF_UART_ID]):
            raise cv.Invalid(
                f"uart_id must be the UART bridged by '{bridge_id}' "
                f"('{bridge_conf[CONF_UART_ID]}').",
                [CONF_UART_ID],
            )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    uart_component = await cg.get_variable(config[CONF_UART_ID])
    bridge = await cg.get_variable(config[CONF_BRIDGE_ID])
    var = cg.new_Pvariable(config[CONF_ID], uart_component, bridge)
    await cg.register_component(var, config)
    if config[CONF_INITIAL_ROUTE] == ROUTE_LOCAL:
        cg.add(var.set_start_local(True))


UART_MUX_ACTION_SCHEMA = automation.maybe_simple_id(
    {cv.Required(CONF_ID): cv.use_id(UARTMux)}
)


@automation.register_action(
    "uart_mux.select_local", SelectLocalAction, UART_MUX_ACTION_SCHEMA, synchronous=True
)
@automation.register_action(
    "uart_mux.select_bridge",
    SelectBridgeAction,
    UART_MUX_ACTION_SCHEMA,
    synchronous=True,
)
async def uart_mux_select_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_condition(
    "uart_mux.is_local", IsLocalCondition, UART_MUX_ACTION_SCHEMA
)
async def uart_mux_is_local_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren)
