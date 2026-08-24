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
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor
from esphome.types import ConfigType, TemplateArgsType

from . import (
    CONF_MITSUBISHI_CN105_ID,
    DOMAIN,
    MITSUBISHI_CN105_DEVICE_SCHEMA,
    MitsubishiCN105Component,
    mitsubishi_ns,
    register_mitsubishi_cn105_device,
)

# Legacy climate-owned hub compatibility. Remove in 2027.2.0.
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["climate"]

_LOGGER = logging.getLogger(__name__)

# Deprecated legacy climate-owned hub option. Remove in 2027.2.0.
CONF_CURRENT_TEMPERATURE_MIN_INTERVAL = "current_temperature_min_interval"
# Legacy climate-owned hub compatibility. Remove in 2027.2.0.
CONF_LEGACY_MITSUBISHI_CN105_ID = "legacy_mitsubishi_cn105_id"

MitsubishiCN105Climate = mitsubishi_ns.class_(
    "MitsubishiCN105Climate",
    climate.Climate,
    cg.Component,
    cg.Parented.template(MitsubishiCN105Component),
)

# Legacy climate action compatibility. Remove in 2027.2.0.
LegacySetRemoteTemperatureAction = mitsubishi_ns.class_(
    "LegacySetRemoteTemperatureAction",
    automation.Action,
    cg.Parented.template(MitsubishiCN105Climate),
)

# Legacy climate action compatibility. Remove in 2027.2.0.
LegacyClearRemoteTemperatureAction = mitsubishi_ns.class_(
    "LegacyClearRemoteTemperatureAction",
    automation.Action,
    cg.Parented.template(MitsubishiCN105Climate),
)


# Legacy climate-owned hub compatibility. Remove in 2027.2.0.
def _has_top_level_hub_config() -> bool:
    return DOMAIN in (CORE.raw_config or {})


# Legacy climate-owned hub compatibility. Remove in 2027.2.0.
def _prepare_legacy_hub_config(config: ConfigType) -> ConfigType:
    _LOGGER.warning(
        "Defining 'climate.mitsubishi_cn105' without a top-level '%s:' hub is "
        "deprecated. Declare '%s:' and reference it with '%s:' instead. Will "
        "be removed in ESPHome 2027.2.0.",
        DOMAIN,
        DOMAIN,
        CONF_MITSUBISHI_CN105_ID,
    )

    # Add the hidden hub declaration only for legacy climate-owned configs,
    # so normal auto-ID resolution does not see it as a top-level hub.
    config[CONF_LEGACY_MITSUBISHI_CN105_ID] = cv.declare_id(MitsubishiCN105Component)(
        None
    )
    return config


_BASE_SCHEMA = climate.climate_schema(MitsubishiCN105Climate).extend(
    {
        cv.Optional(
            CONF_SUPPORTED_SWING_MODES, default="OFF"
        ): validate_climate_swing_mode,
    }
)

_HUB_SCHEMA = _BASE_SCHEMA.extend(MITSUBISHI_CN105_DEVICE_SCHEMA)

# Hub options accepted in the legacy climate-owned configuration. When a
# top-level hub exists, leaving these on the climate is always a migration
# mistake and the generic schema error does not explain where they belong.
# Legacy climate-owned hub compatibility. Remove in 2027.2.0.
_LEGACY_HUB_KEYS = (
    CONF_CURRENT_TEMPERATURE_MIN_INTERVAL,
    CONF_UART_ID,
    CONF_UPDATE_INTERVAL,
)


# Legacy climate-owned hub compatibility. Remove in 2027.2.0.
def _validate_no_legacy_hub_keys(config: ConfigType) -> ConfigType:
    legacy_keys = [key for key in _LEGACY_HUB_KEYS if key in config]
    if not legacy_keys:
        return config

    keys = ", ".join(f"'{key}'" for key in legacy_keys)
    message = f"{keys} must be moved under the top-level '{DOMAIN}:' block"
    if CONF_CURRENT_TEMPERATURE_MIN_INTERVAL in legacy_keys:
        message += (
            f"; rename '{CONF_CURRENT_TEMPERATURE_MIN_INTERVAL}' to "
            "'telemetry_request_min_interval' there"
        )
    raise cv.Invalid(message)


# Legacy climate-owned hub compatibility. Remove in 2027.2.0.
_LEGACY_SCHEMA = (
    _BASE_SCHEMA.extend(uart.UART_DEVICE_SCHEMA)
    .extend(
        {
            cv.Optional(CONF_CURRENT_TEMPERATURE_MIN_INTERVAL): cv.update_interval,
            cv.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
        }
    )
    .add_extra(_prepare_legacy_hub_config)
)


@schema_extractor("schema")
def CONFIG_SCHEMA(config: ConfigType) -> ConfigType:
    if config is SCHEMA_EXTRACT:
        return _HUB_SCHEMA
    if CONF_MITSUBISHI_CN105_ID in config or _has_top_level_hub_config():
        return _HUB_SCHEMA(_validate_no_legacy_hub_keys(config))
    return _LEGACY_SCHEMA(config)


# Legacy climate-owned hub compatibility. Remove in 2027.2.0.
def _legacy_final_validate(config: ConfigType) -> None:
    if CONF_MITSUBISHI_CN105_ID in config:
        return

    uart.final_validate_device_schema(
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
    climate_config = config.copy()
    # update_interval configures the protocol hub, not the climate entity.
    climate_config.pop(CONF_UPDATE_INTERVAL, None)
    await cg.register_component(var, climate_config)
    if CONF_MITSUBISHI_CN105_ID in config:
        await register_mitsubishi_cn105_device(var, config)
    else:
        # Legacy climate-owned hub compatibility. Remove in 2027.2.0.
        parent = cg.new_Pvariable(config[CONF_LEGACY_MITSUBISHI_CN105_ID])
        await cg.register_component(parent, config)
        await uart.register_uart_device(parent, config)
        if CONF_CURRENT_TEMPERATURE_MIN_INTERVAL in config:
            cg.add(
                parent.set_telemetry_request_min_interval(
                    config[CONF_CURRENT_TEMPERATURE_MIN_INTERVAL]
                )
            )
        cg.add(var.set_parent(parent))
    cg.add(var.set_supported_swing_mode(config[CONF_SUPPORTED_SWING_MODES]))


# Legacy climate action compatibility. Remove in 2027.2.0.
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

# Legacy climate action compatibility. Remove in 2027.2.0.
LEGACY_CLEAR_REMOTE_TEMPERATURE_ACTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(MitsubishiCN105Climate),
    }
)


# Legacy climate action compatibility. Remove in 2027.2.0.
@automation.register_action(
    f"climate.{DOMAIN}.set_remote_temperature",
    LegacySetRemoteTemperatureAction,
    LEGACY_REMOTE_TEMPERATURE_ACTION_SCHEMA,
    synchronous=True,
)
async def legacy_remote_temperature_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    _LOGGER.warning(
        "The 'climate.%s.set_remote_temperature' action is deprecated. Use "
        "'%s.set_remote_temperature' instead. It will be removed in ESPHome "
        "2027.2.0.",
        DOMAIN,
        DOMAIN,
    )
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    temperature = await cg.templatable(config[CONF_TEMPERATURE], args, float)
    cg.add(var.set_temperature(temperature))
    return var


# Legacy climate action compatibility. Remove in 2027.2.0.
@automation.register_action(
    f"climate.{DOMAIN}.clear_remote_temperature",
    LegacyClearRemoteTemperatureAction,
    LEGACY_CLEAR_REMOTE_TEMPERATURE_ACTION_SCHEMA,
    synchronous=True,
)
async def legacy_clear_temperature_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    _LOGGER.warning(
        "The 'climate.%s.clear_remote_temperature' action is deprecated. Use "
        "'%s.clear_remote_temperature' instead. It will be removed in ESPHome "
        "2027.2.0.",
        DOMAIN,
        DOMAIN,
    )
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
