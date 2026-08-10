from esphome import automation
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TEMPERATURE, CONF_UPDATE_INTERVAL
from esphome.core import ID
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType, TemplateArgsType

CODEOWNERS = ["@crnjan"]
DEPENDENCIES = ["uart"]
DOMAIN = "mitsubishi_cn105"

CONF_MITSUBISHI_CN105_ID = f"{DOMAIN}_id"
CONF_TELEMETRY_REQUEST_MIN_INTERVAL = "telemetry_request_min_interval"

mitsubishi_ns = cg.esphome_ns.namespace(DOMAIN)

MitsubishiCN105Component = mitsubishi_ns.class_(
    "MitsubishiCN105Component",
    cg.Component,
    uart.UARTDevice,
)

SetRemoteTemperatureAction = mitsubishi_ns.class_(
    "SetRemoteTemperatureAction",
    automation.Action,
    cg.Parented.template(MitsubishiCN105Component),
)

ClearRemoteTemperatureAction = mitsubishi_ns.class_(
    "ClearRemoteTemperatureAction",
    automation.Action,
    cg.Parented.template(MitsubishiCN105Component),
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MitsubishiCN105Component),
            cv.Optional(CONF_UPDATE_INTERVAL, default="1s"): cv.update_interval,
            cv.Optional(
                CONF_TELEMETRY_REQUEST_MIN_INTERVAL, default="60s"
            ): cv.update_interval,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

MITSUBISHI_CN105_DEVICE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MITSUBISHI_CN105_ID): cv.use_id(MitsubishiCN105Component),
    }
)

FINAL_VALIDATE_SCHEMA = cv.All(
    uart.final_validate_device_schema(
        DOMAIN,
        require_rx=True,
        require_tx=True,
        data_bits=8,
        parity="EVEN",
        stop_bits=1,
    )
)


async def register_mitsubishi_cn105_device(var: MockObj, config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_MITSUBISHI_CN105_ID])
    cg.add(var.set_parent(parent))


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(
        var.set_telemetry_request_min_interval(
            config[CONF_TELEMETRY_REQUEST_MIN_INTERVAL]
        )
    )


REMOTE_TEMPERATURE_ACTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(MitsubishiCN105Component),
        cv.Required(CONF_TEMPERATURE): cv.templatable(
            cv.All(
                cv.temperature,
                cv.Range(min=8.0, max=39.5),
            )
        ),
    }
)

CLEAR_REMOTE_TEMPERATURE_ACTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(MitsubishiCN105Component),
    }
)


@automation.register_action(
    f"{DOMAIN}.set_remote_temperature",
    SetRemoteTemperatureAction,
    REMOTE_TEMPERATURE_ACTION_SCHEMA,
    synchronous=True,
)
async def remote_temperature_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    temperature = await cg.templatable(config[CONF_TEMPERATURE], args, float)
    cg.add(var.set_temperature(temperature))
    return var


@automation.register_action(
    f"{DOMAIN}.clear_remote_temperature",
    ClearRemoteTemperatureAction,
    CLEAR_REMOTE_TEMPERATURE_ACTION_SCHEMA,
    synchronous=True,
)
async def clear_temperature_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
