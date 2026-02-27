import logging

import esphome.codegen as cg
from esphome.components import time as time_
from esphome.config_helpers import merge_config
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_PLATFORM,
    CONF_SERVERS,
    CONF_TIME,
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_LN882X,
    PLATFORM_RP2040,
    PLATFORM_RTL87XX,
)
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

DEPENDENCIES = ["network"]

CONF_SNTP = "sntp"

sntp_ns = cg.esphome_ns.namespace("sntp")
SNTPComponent = sntp_ns.class_("SNTPComponent", time_.RealTimeClock)

DEFAULT_SERVERS = ["0.pool.ntp.org", "1.pool.ntp.org", "2.pool.ntp.org"]


def _sntp_final_validate(config: ConfigType) -> None:
    """Merge multiple SNTP instances into one, similar to OTA merging behavior."""
    full_conf = fv.full_config.get()
    time_confs = full_conf.get(CONF_TIME, [])

    sntp_configs: list[ConfigType] = []
    other_time_configs: list[ConfigType] = []

    for time_conf in time_confs:
        if time_conf.get(CONF_PLATFORM) == CONF_SNTP:
            sntp_configs.append(time_conf)
        else:
            other_time_configs.append(time_conf)

    if len(sntp_configs) <= 1:
        return

    # Merge all SNTP configs into the first one
    merged = sntp_configs[0]
    for sntp_conf in sntp_configs[1:]:
        # Validate that IDs are consistent if manually specified
        if merged[CONF_ID].is_manual and sntp_conf[CONF_ID].is_manual:
            raise cv.Invalid(
                f"Found multiple SNTP configurations but {CONF_ID} is inconsistent"
            )
        merged = merge_config(merged, sntp_conf)

    # Deduplicate servers while preserving order
    servers = merged[CONF_SERVERS]
    unique_servers = list(dict.fromkeys(servers))

    # Warn if we're dropping servers due to 3-server limit
    if len(unique_servers) > 3:
        dropped = unique_servers[3:]
        unique_servers = unique_servers[:3]
        _LOGGER.warning(
            "SNTP supports maximum 3 servers. Dropped excess server(s): %s",
            dropped,
        )

    merged[CONF_SERVERS] = unique_servers

    _LOGGER.warning(
        "Found and merged %d SNTP time configurations into one instance",
        len(sntp_configs),
    )

    # Replace time configs with merged SNTP + other time platforms
    other_time_configs.append(merged)
    full_conf[CONF_TIME] = other_time_configs
    fv.full_config.set(full_conf)


CONFIG_SCHEMA = cv.All(
    time_.TIME_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(SNTPComponent),
            cv.Optional(CONF_SERVERS, default=DEFAULT_SERVERS): cv.All(
                cv.ensure_list(cv.Any(cv.domain, cv.hostname)), cv.Length(min=1, max=3)
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on(
        [
            PLATFORM_ESP32,
            PLATFORM_ESP8266,
            PLATFORM_RP2040,
            PLATFORM_BK72XX,
            PLATFORM_LN882X,
            PLATFORM_RTL87XX,
        ]
    ),
)

FINAL_VALIDATE_SCHEMA = _sntp_final_validate


async def to_code(config):
    servers = config[CONF_SERVERS]

    # Define server count at compile time
    cg.add_define("SNTP_SERVER_COUNT", len(servers))

    # Pass string literals to constructor - stored in flash/rodata by compiler
    var = cg.new_Pvariable(config[CONF_ID], servers)

    await cg.register_component(var, config)
    await time_.register_time(var, config)

    if CORE.is_esp8266 and len(servers) > 1:
        # We need LwIP features enabled to get 3 SNTP servers (not just one)
        cg.add_build_flag("-DPIO_FRAMEWORK_ARDUINO_LWIP2_LOW_MEMORY")
