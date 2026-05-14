from esphome import automation
import esphome.codegen as cg
from esphome.components import climate, uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TEMPERATURE, CONF_UPDATE_INTERVAL
from esphome.core import ID
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType, TemplateArgsType

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["climate"]
CODEOWNERS = ["@crnjan"]

CONF_CURRENT_TEMPERATURE_MIN_INTERVAL = "current_temperature_min_interval"

mitsubishi_ns = cg.esphome_ns.namespace("mitsubishi_cn105")

MitsubishiCN105Climate = mitsubishi_ns.class_(
    "MitsubishiCN105Climate",
    climate.Climate,
    cg.Component,
    uart.UARTDevice,
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
    cg.add(
        var.set_current_temperature_min_interval(
            config[CONF_CURRENT_TEMPERATURE_MIN_INTERVAL]
        )
    )


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
