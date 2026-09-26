from esphome import automation
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import ID
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType, TemplateArgsType

CODEOWNERS = ["@ljungqvist"]

MULTI_CONF = True

DEPENDENCIES = ["uart"]

ufm01_ns = cg.esphome_ns.namespace("ufm01")
UFM01Component = ufm01_ns.class_("UFM01Component", uart.UARTDevice, cg.Component)
ClearAccumulatedFlowAction = ufm01_ns.class_(
    "ClearAccumulatedFlowAction", automation.Action
)

CONF_UFM01_ID = "ufm01_id"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(UFM01Component),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

CLEAR_ACCUMULATED_FLOW_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(UFM01Component),
    }
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "ufm01",
    require_tx=True,
    require_rx=True,
    baud_rate=2400,
    data_bits=8,
    parity="EVEN",
    stop_bits=1,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)


@automation.register_action(
    "ufm01.clear_accumulated_flow",
    ClearAccumulatedFlowAction,
    CLEAR_ACCUMULATED_FLOW_ACTION_SCHEMA,
    synchronous=False,
)
async def clear_accumulated_flow_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    cg.add_define("USE_UFM01_CLEAR_ACCUMULATED_FLOW_ACTION")
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)
