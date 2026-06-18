import logging

from esphome import automation
import esphome.codegen as cg
from esphome.components import climate, uart
from esphome.components.climate import validate_climate_swing_mode
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_SUPPORTED_SWING_MODES,
    CONF_TEMPERATURE,
    CONF_UART_ID,
    CONF_UPDATE_INTERVAL,
)
from esphome.core import CORE, ID
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType, TemplateArgsType

from . import (
    CONF_MITSUBISHI_CN105_ID,
    CONF_TELEMETRY_REQUEST_MIN_INTERVAL,
    DOMAIN,
    MITSUBISHI_CN105_DEVICE_SCHEMA,
    ClearRemoteTemperatureAction,
    MitsubishiCN105Component,
    SetRemoteTemperatureAction,
    clear_remote_temperature_action_to_code,
    mitsubishi_ns,
    register_mitsubishi_cn105_device,
    set_remote_temperature_action_to_code,
)

AUTO_LOAD = ["climate"]

_LOGGER = logging.getLogger(__name__)

# Deprecated legacy climate-owned hub option. Remove in 2027.1.0.
CONF_CURRENT_TEMPERATURE_MIN_INTERVAL = "current_temperature_min_interval"
# Legacy climate-owned hub compatibility. Remove in 2027.1.0.
CONF_LEGACY_MITSUBISHI_CN105_ID = "legacy_mitsubishi_cn105_id"

MitsubishiCN105Climate = mitsubishi_ns.class_(
    "MitsubishiCN105Climate",
    climate.Climate,
    cg.Component,
    cg.Parented.template(MitsubishiCN105Component),
)


# Legacy climate-owned hub compatibility. Remove in 2027.1.0.
def _has_top_level_hub_config() -> bool:
    return DOMAIN in getattr(CORE, "raw_config", {})


# Legacy climate-owned hub compatibility. Remove in 2027.1.0.
def _validate_legacy_options(config: ConfigType) -> ConfigType:
    hub_id = config[CONF_MITSUBISHI_CN105_ID]
    if not hub_id.is_manual and not _has_top_level_hub_config():
        return config

    if CONF_UPDATE_INTERVAL in config:
        raise cv.Invalid(
            f"'{CONF_UPDATE_INTERVAL}' cannot be used with '{CONF_MITSUBISHI_CN105_ID}'. "
            f"Use top-level '{DOMAIN}.{CONF_UPDATE_INTERVAL}' instead."
        )

    if CONF_CURRENT_TEMPERATURE_MIN_INTERVAL in config:
        raise cv.Invalid(
            f"'{CONF_CURRENT_TEMPERATURE_MIN_INTERVAL}' cannot be used with "
            f"'{CONF_MITSUBISHI_CN105_ID}'. Use top-level "
            f"'{DOMAIN}.{CONF_TELEMETRY_REQUEST_MIN_INTERVAL}' instead."
        )

    if config[CONF_UART_ID].is_manual:
        raise cv.Invalid(
            f"'{CONF_UART_ID}' cannot be used with '{CONF_MITSUBISHI_CN105_ID}'. "
            f"Move it to the top-level '{DOMAIN}:' component."
        )
    return config


# Legacy climate-owned hub compatibility. Remove in 2027.1.0.
def _prepare_legacy_hub_config(config: ConfigType) -> ConfigType:
    hub_id = config[CONF_MITSUBISHI_CN105_ID]
    if hub_id.is_manual or _has_top_level_hub_config():
        return config

    _LOGGER.warning(
        "Configuring Mitsubishi CN105 UART/protocol options under "
        "'climate.mitsubishi_cn105' is deprecated. Use top-level "
        "'%s:' and reference it with '%s:' instead. Will be removed in "
        "ESPHome 2027.1.0.",
        DOMAIN,
        CONF_MITSUBISHI_CN105_ID,
    )

    config.pop(CONF_MITSUBISHI_CN105_ID)
    # Add the hidden hub declaration only for legacy climate-owned configs,
    # so normal auto-ID resolution does not see it as a top-level hub.
    config[CONF_LEGACY_MITSUBISHI_CN105_ID] = cv.declare_id(MitsubishiCN105Component)(
        None
    )
    return config


CONFIG_SCHEMA = (
    climate.climate_schema(MitsubishiCN105Climate)
    .extend(MITSUBISHI_CN105_DEVICE_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(
        {
            cv.Optional(CONF_CURRENT_TEMPERATURE_MIN_INTERVAL): cv.update_interval,
            cv.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
            cv.Optional(
                CONF_SUPPORTED_SWING_MODES, default="OFF"
            ): validate_climate_swing_mode,
        }
    )
    .add_extra(_validate_legacy_options)
    .add_extra(_prepare_legacy_hub_config)
)


# Legacy climate-owned hub compatibility. Remove in 2027.1.0.
def _legacy_final_validate(config: ConfigType) -> ConfigType:
    if CONF_MITSUBISHI_CN105_ID in config:
        return config

    return uart.final_validate_device_schema(
        DOMAIN,
        require_rx=True,
        require_tx=True,
        data_bits=8,
        parity="EVEN",
        stop_bits=1,
    )(config)


FINAL_VALIDATE_SCHEMA = _legacy_final_validate


async def to_code(config: ConfigType) -> None:
    var = await climate.new_climate(config)
    await cg.register_component(var, config)
    if CONF_MITSUBISHI_CN105_ID in config:
        await register_mitsubishi_cn105_device(var, config)
    else:
        parent = cg.new_Pvariable(config[CONF_LEGACY_MITSUBISHI_CN105_ID])
        await cg.register_component(parent, config)
        await uart.register_uart_device(parent, config)
        if CONF_CURRENT_TEMPERATURE_MIN_INTERVAL in config:
            cg.add(
                parent.set_telemetry_request_min_interval(
                    config[CONF_CURRENT_TEMPERATURE_MIN_INTERVAL]
                )
            )
        if CONF_UPDATE_INTERVAL in config:
            cg.add(parent.set_update_interval(config[CONF_UPDATE_INTERVAL]))
        cg.add(var.set_parent(parent))
    cg.add(var.set_supported_swing_mode(config[CONF_SUPPORTED_SWING_MODES]))


# Legacy climate action compatibility. Remove in 2027.1.0.
async def _get_legacy_action_parent(config: ConfigType) -> MockObj:
    climate_var = await cg.get_variable(config[CONF_ID])
    return climate_var.get_parent()


LEGACY_REMOTE_TEMPERATURE_ACTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(MitsubishiCN105Climate),
        cv.Required(CONF_TEMPERATURE): cv.templatable(
            cv.All(
                cv.temperature,
                cv.Range(min=8.0, max=39.5),
            )
        ),
    }
)

LEGACY_CLEAR_REMOTE_TEMPERATURE_ACTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(MitsubishiCN105Climate),
    }
)


# Legacy climate action compatibility. Remove in 2027.1.0.
@automation.register_action(
    f"climate.{DOMAIN}.set_remote_temperature",
    SetRemoteTemperatureAction,
    LEGACY_REMOTE_TEMPERATURE_ACTION_SCHEMA,
    synchronous=True,
)
async def legacy_remote_temperature_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    parent = await _get_legacy_action_parent(config)
    return await set_remote_temperature_action_to_code(
        config, action_id, template_arg, args, parent
    )


# Legacy climate action compatibility. Remove in 2027.1.0.
@automation.register_action(
    f"climate.{DOMAIN}.clear_remote_temperature",
    ClearRemoteTemperatureAction,
    LEGACY_CLEAR_REMOTE_TEMPERATURE_ACTION_SCHEMA,
    synchronous=True,
)
async def legacy_clear_temperature_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    parent = await _get_legacy_action_parent(config)
    return await clear_remote_temperature_action_to_code(
        action_id, template_arg, parent
    )
