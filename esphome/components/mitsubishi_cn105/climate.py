from esphome import automation
import esphome.codegen as cg
from esphome.components import climate, select, uart
from esphome.components.climate import validate_climate_swing_mode
import esphome.config_validation as cv
from esphome.const import (
    CONF_DIRECTION,
    CONF_ID,
    CONF_ON_STATE,
    CONF_SUPPORTED_SWING_MODES,
    CONF_TEMPERATURE,
    CONF_UPDATE_INTERVAL,
)
from esphome.core import ID, Lambda
from esphome.cpp_generator import LambdaExpression, MockObj
from esphome.types import ConfigType, TemplateArgsType

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["climate", "select"]
CODEOWNERS = ["@crnjan"]

CONF_CURRENT_TEMPERATURE_MIN_INTERVAL = "current_temperature_min_interval"
CONF_VANE = "vane"
CONF_VERTICAL = "vertical"

mitsubishi_ns = cg.esphome_ns.namespace("mitsubishi_cn105")

MitsubishiCN105Climate = mitsubishi_ns.class_(
    "MitsubishiCN105Climate",
    climate.Climate,
    cg.Component,
    uart.UARTDevice,
)

VaneState = mitsubishi_ns.struct("VaneState")

VaneCall = mitsubishi_ns.class_("VaneCall")

VerticalVaneMode = mitsubishi_ns.enum("VerticalVaneMode")

# NOTE: The insertion order of these options must match the VALUES array in
# mitsubishi_cn105_vane_select_vertical.cpp. The select uses index-based
# control/state publishing, which is the preferred ESPHome Select API.
VERTICAL_VANE_DIRECTIONS = {
    "AUTO": VerticalVaneMode.VERTICAL_VANE_MODE_AUTO,
    "1": VerticalVaneMode.VERTICAL_VANE_MODE_POSITION_1,
    "2": VerticalVaneMode.VERTICAL_VANE_MODE_POSITION_2,
    "3": VerticalVaneMode.VERTICAL_VANE_MODE_POSITION_3,
    "4": VerticalVaneMode.VERTICAL_VANE_MODE_POSITION_4,
    "5": VerticalVaneMode.VERTICAL_VANE_MODE_POSITION_5,
    "SWING": VerticalVaneMode.VERTICAL_VANE_MODE_SWING,
}

MitsubishiCN105VerticalVaneDirectionSelect = mitsubishi_ns.class_(
    "MitsubishiCN105VerticalVaneDirectionSelect",
    select.Select,
)

VaneControlAction = mitsubishi_ns.class_(
    "VaneControlAction",
    automation.Action,
)

SetRemoteTemperatureAction = mitsubishi_ns.class_(
    "SetRemoteTemperatureAction",
    automation.Action,
    cg.Parented.template(MitsubishiCN105Climate),
)

ClearRemoteTemperatureAction = mitsubishi_ns.class_(
    "ClearRemoteTemperatureAction",
    automation.Action,
    cg.Parented.template(MitsubishiCN105Climate),
)

CONFIG_SCHEMA = (
    climate.climate_schema(MitsubishiCN105Climate)
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(
        {
            cv.Optional(CONF_UPDATE_INTERVAL, default="1s"): cv.update_interval,
            cv.Optional(
                CONF_CURRENT_TEMPERATURE_MIN_INTERVAL, default="60s"
            ): cv.update_interval,
            cv.Optional(
                CONF_SUPPORTED_SWING_MODES, default="OFF"
            ): validate_climate_swing_mode,
            cv.Optional(CONF_VANE): cv.Schema(
                {
                    cv.Optional(CONF_VERTICAL): cv.Schema(
                        {
                            cv.Optional(CONF_DIRECTION): select.select_schema(
                                MitsubishiCN105VerticalVaneDirectionSelect,
                                icon="mdi:arrow-up-down",
                            ),
                        }
                    ),
                    cv.Optional(CONF_ON_STATE): automation.validate_automation({}),
                }
            ),
        }
    )
)

FINAL_VALIDATE_SCHEMA = cv.All(
    uart.final_validate_device_schema(
        "mitsubishi_cn105",
        require_rx=True,
        require_tx=True,
        data_bits=8,
        parity="EVEN",
        stop_bits=1,
    )
)


async def to_code(config: ConfigType) -> None:
    var = await climate.new_climate(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_supported_swing_mode(config[CONF_SUPPORTED_SWING_MODES]))
    cg.add(
        var.set_current_temperature_min_interval(
            config[CONF_CURRENT_TEMPERATURE_MIN_INTERVAL]
        )
    )
    if vane := config.get(CONF_VANE):
        if (vertical := vane.get(CONF_VERTICAL)) and (
            direction_conf := vertical.get(CONF_DIRECTION)
        ):
            sel = cg.new_Pvariable(direction_conf[CONF_ID])
            await select.register_select(
                sel,
                direction_conf,
                options=[option.capitalize() for option in VERTICAL_VANE_DIRECTIONS],
            )
            await cg.register_parented(sel, var)
            cg.add(var.set_vertical_vane_direction_select(sel))
        for conf in vane.get(CONF_ON_STATE, []):
            await automation.build_callback_automation(
                var,
                "add_on_vane_state_callback",
                [(VaneState.operator("const").operator("ref"), "x")],
                conf,
            )


VANE_CONTROL_FIELDS = (
    (
        (CONF_VERTICAL, CONF_DIRECTION),
        "vertical.set_direction",
        VerticalVaneMode,
    ),
)

VANE_CONTROL_ACTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(MitsubishiCN105Climate),
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
    "climate.mitsubishi_cn105.vane.control",
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
    parent = await cg.get_variable(config[CONF_ID])

    normalized_args = [
        (cg.RawExpression(f"const std::remove_cvref_t<{cg.safe_exp(t)}> &"), n)
        for t, n in args
    ]

    fwd_args = ", ".join(name for _, name in args)
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
            body_lines.append(f"call.{setter}(({inner})({fwd_args}));")
        else:
            body_lines.append(f"call.{setter}({cg.safe_exp(value)});")

    apply_args = [
        (VaneCall.operator("ref"), "call"),
        *normalized_args,
    ]
    apply_lambda = LambdaExpression(
        ["\n".join(body_lines)],
        apply_args,
        capture="",
        return_type=cg.void,
    )

    return cg.new_Pvariable(action_id, template_arg, parent, apply_lambda)


@automation.register_action(
    "climate.mitsubishi_cn105.set_remote_temperature",
    SetRemoteTemperatureAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(MitsubishiCN105Climate),
            cv.Required(CONF_TEMPERATURE): cv.templatable(
                cv.All(
                    cv.temperature,
                    cv.Range(min=8.0, max=39.5),
                )
            ),
        }
    ),
    synchronous=True,
)
async def set_remote_temperature_action_to_code(
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
    "climate.mitsubishi_cn105.clear_remote_temperature",
    ClearRemoteTemperatureAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(MitsubishiCN105Climate),
        }
    ),
    synchronous=True,
)
async def clear_remote_temperature_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
