import logging

import esphome.codegen as cg
from esphome.components import climate, uart
from esphome.components.climate import validate_climate_swing_mode
import esphome.config_validation as cv
from esphome.const import CONF_SUPPORTED_SWING_MODES, CONF_UART_ID, CONF_UPDATE_INTERVAL
from esphome.core import CORE
from esphome.types import ConfigType

from . import (
    CONF_MITSUBISHI_CN105_ID,
    CONF_TELEMETRY_REQUEST_MIN_INTERVAL,
    DOMAIN,
    MITSUBISHI_CN105_DEVICE_SCHEMA,
    MitsubishiCN105Component,
    mitsubishi_ns,
    register_mitsubishi_cn105_device,
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
