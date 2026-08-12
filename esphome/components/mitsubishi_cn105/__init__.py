from esphome import automation
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_DIRECTION,
    CONF_ID,
    CONF_ON_STATE,
    CONF_TEMPERATURE,
    CONF_UPDATE_INTERVAL,
)
from esphome.core import ID, Lambda
from esphome.cpp_generator import LambdaExpression, MockObj
from esphome.types import ConfigType, TemplateArgsType

CODEOWNERS = ["@crnjan"]
DEPENDENCIES = ["uart"]
DOMAIN = "mitsubishi_cn105"

CONF_MITSUBISHI_CN105_ID = f"{DOMAIN}_id"
CONF_TELEMETRY_REQUEST_MIN_INTERVAL = "telemetry_request_min_interval"
CONF_VANE = "vane"
CONF_VERTICAL = "vertical"

mitsubishi_ns = cg.esphome_ns.namespace(DOMAIN)

MitsubishiCN105Component = mitsubishi_ns.class_(
    "MitsubishiCN105Component",
    cg.Component,
    uart.UARTDevice,
)

VaneState = mitsubishi_ns.struct("VaneState")
VaneCall = mitsubishi_ns.class_("VaneCall")
VerticalVaneMode = mitsubishi_ns.enum("VerticalVaneMode")

# The insertion order must match VALUES in
# select/mitsubishi_cn105_vane_select_vertical.cpp.
VERTICAL_VANE_DIRECTIONS = {
    "AUTO": VerticalVaneMode.VERTICAL_VANE_MODE_AUTO,
    "1": VerticalVaneMode.VERTICAL_VANE_MODE_POSITION_1,
    "2": VerticalVaneMode.VERTICAL_VANE_MODE_POSITION_2,
    "3": VerticalVaneMode.VERTICAL_VANE_MODE_POSITION_3,
    "4": VerticalVaneMode.VERTICAL_VANE_MODE_POSITION_4,
    "5": VerticalVaneMode.VERTICAL_VANE_MODE_POSITION_5,
    "SWING": VerticalVaneMode.VERTICAL_VANE_MODE_SWING,
}

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

VaneControlAction = mitsubishi_ns.class_(
    "VaneControlAction",
    automation.Action,
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MitsubishiCN105Component),
            cv.Optional(CONF_UPDATE_INTERVAL, default="1s"): cv.update_interval,
            cv.Optional(
                CONF_TELEMETRY_REQUEST_MIN_INTERVAL, default="60s"
            ): cv.update_interval,
            cv.Optional(CONF_VANE): cv.Schema(
                {
                    cv.Optional(CONF_ON_STATE): automation.validate_automation({}),
                }
            ),
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
    if on_state := config.get(CONF_VANE, {}).get(CONF_ON_STATE):
        cg.add_global(mitsubishi_ns.using)
        for conf in on_state:
            await automation.build_callback_automation(
                var,
                "add_on_vane_state_callback",
                [(VaneState.operator("const").operator("ref"), "x")],
                conf,
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


VANE_CONTROL_FIELDS = (
    (
        (CONF_VERTICAL, CONF_DIRECTION),
        "vertical.set_direction",
        VerticalVaneMode,
    ),
)

VANE_CONTROL_ACTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(MitsubishiCN105Component),
        cv.Optional(CONF_VERTICAL): cv.Schema(
            {
                cv.Optional(CONF_DIRECTION): cv.templatable(
                    cv.enum(VERTICAL_VANE_DIRECTIONS, upper=True)
                ),
            }
        ),
    }
)


@automation.register_action(
    f"{DOMAIN}.vane.control",
    VaneControlAction,
    VANE_CONTROL_ACTION_SCHEMA,
    synchronous=True,
)
async def vane_control_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    cg.add_global(mitsubishi_ns.using)
    parent = await cg.get_variable(config[CONF_ID])
    normalized_args = [
        (cg.RawExpression(f"const std::remove_cvref_t<{cg.safe_exp(t)}> &"), name)
        for t, name in args
    ]
    forwarded_args = ", ".join(name for _, name in args)
    body_lines: list[str] = []

    for path, setter, type_ in VANE_CONTROL_FIELDS:
        if (section := config.get(path[0])) is None:
            continue
        if (value := section.get(path[1])) is None:
            continue
        if isinstance(value, Lambda):
            inner = await cg.process_lambda(
                value,
                normalized_args,
                return_type=type_,
            )
            body_lines.append(f"call.{setter}(({inner})({forwarded_args}));")
        else:
            body_lines.append(f"call.{setter}({cg.safe_exp(value)});")

    apply_lambda = LambdaExpression(
        ["\n".join(body_lines)],
        [(VaneCall.operator("ref"), "call"), *normalized_args],
        capture="",
        return_type=cg.void,
    )
    return cg.new_Pvariable(action_id, template_arg, parent, apply_lambda)
