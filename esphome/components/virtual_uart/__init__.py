from esphome import automation
import esphome.codegen as cg
from esphome.components import uart
from esphome.components.const import CONF_DATA_BITS, CONF_PARITY, CONF_STOP_BITS
import esphome.config_validation as cv
from esphome.const import (
    CONF_BAUD_RATE,
    CONF_DATA,
    CONF_DEBUG,
    CONF_ID,
    CONF_RX_BUFFER_SIZE,
)
from esphome.core import ID
from esphome.cpp_generator import MockObj, TemplateArgsType
from esphome.types import ConfigType

CODEOWNERS = ["@Bascht74"]
DOMAIN = "virtual_uart"
AUTO_LOAD = ["uart"]
MULTI_CONF = True

CONF_ON_TX = "on_tx"

virtual_uart_ns = cg.esphome_ns.namespace("virtual_uart")
VirtualUART = virtual_uart_ns.class_(
    "VirtualUART", uart.VirtualUARTComponent, cg.Component
)
InjectRXAction = virtual_uart_ns.class_("InjectRXAction", automation.Action)

_DATA_SPAN = cg.std_span.template(cg.uint8.operator("const"))


def _synchronous(value: ConfigType) -> ConfigType:
    if automation.has_non_synchronous_actions(value):
        raise cv.Invalid(
            "Deferring actions (delay, wait_until, script.wait, ...) are not allowed in on_tx: "
            "data is only valid while the automation runs. Copy what you need into globals first."
        )
    return value


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(VirtualUART),
        cv.Required(CONF_BAUD_RATE): cv.int_range(min=1),
        cv.Optional(CONF_DATA_BITS, default=8): cv.int_range(min=5, max=8),
        cv.Optional(CONF_PARITY, default="NONE"): cv.enum(
            uart.UART_PARITY_OPTIONS, upper=True
        ),
        cv.Optional(CONF_STOP_BITS, default=1): cv.one_of(1, 2, int=True),
        cv.Optional(CONF_RX_BUFFER_SIZE, default=256): cv.All(
            cv.validate_bytes, cv.int_range(min=1, max=65535)
        ),
        cv.Optional(CONF_ON_TX): cv.All(
            automation.validate_automation(single=True), _synchronous
        ),
        cv.Optional(CONF_DEBUG): uart.maybe_empty_debug,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    uart.require_virtual_uart()
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_RX_BUFFER_SIZE])
    await cg.register_component(var, config)
    cg.add(var.set_baud_rate(config[CONF_BAUD_RATE]))
    cg.add(var.set_data_bits(config[CONF_DATA_BITS]))
    if (parity := config[CONF_PARITY]) != "NONE":
        cg.add(var.set_parity(parity))
    cg.add(var.set_stop_bits(config[CONF_STOP_BITS]))
    if on_tx := config.get(CONF_ON_TX):
        await automation.build_callback_automation(
            var, "add_on_tx_callback", [(_DATA_SPAN, "data")], on_tx
        )
    if debug := config.get(CONF_DEBUG):
        cg.add_global(uart.uart_ns.using)
        await uart.debug_to_code(debug, var)


@automation.register_action(
    "virtual_uart.inject_rx",
    InjectRXAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(VirtualUART),
            cv.Required(CONF_DATA): cv.templatable(uart.validate_raw_data),
        },
        key=CONF_DATA,
    ),
    synchronous=True,
)
async def inject_rx_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    await automation.templatable_bytes(
        config[CONF_DATA],
        args,
        var.set_data_template,
        var.set_data_static,
        "virtual_uart_data",
    )
    return var
