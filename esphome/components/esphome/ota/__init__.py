import logging

import esphome.codegen as cg
from esphome.components.ota import BASE_OTA_SCHEMA, OTAComponent, ota_to_code
from esphome.config_helpers import merge_config
import esphome.config_validation as cv
from esphome.const import (
    CONF_ESPHOME,
    CONF_ID,
    CONF_NUM_ATTEMPTS,
    CONF_OTA,
    CONF_PASSWORD,
    CONF_PLATFORM,
    CONF_PORT,
    CONF_REBOOT_TIMEOUT,
    CONF_SAFE_MODE,
    CONF_VERSION,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.coroutine import CoroPriority
import esphome.final_validate as fv
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)


CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["network"]


def supports_sha256() -> bool:
    """Check if the current platform supports SHA256 for OTA authentication."""
    return bool(CORE.is_esp32 or CORE.is_esp8266 or CORE.is_rp2040 or CORE.is_libretiny)


def AUTO_LOAD() -> list[str]:
    """Conditionally auto-load sha256 only on platforms that support it."""
    base_components = ["md5", "socket"]
    if supports_sha256():
        return base_components + ["sha256"]
    return base_components


esphome = cg.esphome_ns.namespace("esphome")
ESPHomeOTAComponent = esphome.class_("ESPHomeOTAComponent", OTAComponent)


def ota_esphome_final_validate(config):
    full_conf = fv.full_config.get()
    full_ota_conf = full_conf[CONF_OTA]
    new_ota_conf = []
    merged_ota_esphome_configs_by_port = {}
    ports_with_merged_configs = []
    for ota_conf in full_ota_conf:
        if ota_conf.get(CONF_PLATFORM) == CONF_ESPHOME:
            if (
                conf_port := ota_conf.get(CONF_PORT)
            ) not in merged_ota_esphome_configs_by_port:
                merged_ota_esphome_configs_by_port[conf_port] = ota_conf
            else:
                if merged_ota_esphome_configs_by_port[conf_port][
                    CONF_VERSION
                ] != ota_conf.get(CONF_VERSION):
                    raise cv.Invalid(
                        f"Found multiple configurations but {CONF_VERSION} is inconsistent"
                    )
                if (
                    merged_ota_esphome_configs_by_port[conf_port][CONF_ID].is_manual
                    and ota_conf.get(CONF_ID).is_manual
                ):
                    raise cv.Invalid(
                        f"Found multiple configurations but {CONF_ID} is inconsistent"
                    )
                if (
                    CONF_PASSWORD in merged_ota_esphome_configs_by_port[conf_port]
                    and CONF_PASSWORD in ota_conf
                    and merged_ota_esphome_configs_by_port[conf_port][CONF_PASSWORD]
                    != ota_conf.get(CONF_PASSWORD)
                ):
                    raise cv.Invalid(
                        f"Found multiple configurations but {CONF_PASSWORD} is inconsistent"
                    )

                ports_with_merged_configs.append(conf_port)
                merged_ota_esphome_configs_by_port[conf_port] = merge_config(
                    merged_ota_esphome_configs_by_port[conf_port], ota_conf
                )
        else:
            new_ota_conf.append(ota_conf)

    new_ota_conf.extend(merged_ota_esphome_configs_by_port.values())

    full_conf[CONF_OTA] = new_ota_conf
    fv.full_config.set(full_conf)

    if len(ports_with_merged_configs) > 0:
        _LOGGER.warning(
            "Found and merged multiple configurations for %s %s %s port(s) %s",
            CONF_OTA,
            CONF_PLATFORM,
            CONF_ESPHOME,
            ports_with_merged_configs,
        )


def _consume_ota_sockets(config: ConfigType) -> ConfigType:
    """Register socket needs for OTA component."""
    from esphome.components import socket

    # OTA needs 1 listening socket (client connections are temporary during updates)
    socket.consume_sockets(1, "ota")(config)
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ESPHomeOTAComponent),
            cv.Optional(CONF_VERSION, default=2): cv.one_of(1, 2, int=True),
            cv.SplitDefault(
                CONF_PORT,
                esp8266=8266,
                esp32=3232,
                rp2040=2040,
                bk72xx=8892,
                ln882x=8820,
                rtl87xx=8892,
            ): cv.port,
            cv.Optional(CONF_PASSWORD): cv.string,
            cv.Optional(CONF_NUM_ATTEMPTS): cv.invalid(
                f"'{CONF_SAFE_MODE}' (and its related configuration variables) has moved from 'ota' to its own component. See https://esphome.io/components/safe_mode"
            ),
            cv.Optional(CONF_REBOOT_TIMEOUT): cv.invalid(
                f"'{CONF_SAFE_MODE}' (and its related configuration variables) has moved from 'ota' to its own component. See https://esphome.io/components/safe_mode"
            ),
            cv.Optional(CONF_SAFE_MODE): cv.invalid(
                f"'{CONF_SAFE_MODE}' (and its related configuration variables) has moved from 'ota' to its own component. See https://esphome.io/components/safe_mode"
            ),
        }
    )
    .extend(BASE_OTA_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    _consume_ota_sockets,
)

FINAL_VALIDATE_SCHEMA = ota_esphome_final_validate


@coroutine_with_priority(CoroPriority.OTA_UPDATES)
async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_port(config[CONF_PORT]))

    # Password could be set to an empty string and we can assume that means no password
    if config.get(CONF_PASSWORD):
        cg.add(var.set_auth_password(config[CONF_PASSWORD]))
        cg.add_define("USE_OTA_PASSWORD")
        # Only include hash algorithms when password is configured
        cg.add_define("USE_OTA_MD5")
        # Only include SHA256 support on platforms that have it
        if supports_sha256():
            cg.add_define("USE_OTA_SHA256")
    cg.add_define("USE_OTA_VERSION", config[CONF_VERSION])

    await cg.register_component(var, config)
    await ota_to_code(var, config)
