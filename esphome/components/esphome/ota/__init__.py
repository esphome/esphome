import logging

import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components.ota import BASE_OTA_SCHEMA, ota_to_code, OTAComponent
from esphome.config_helpers import merge_config
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
from esphome.core import coroutine_with_priority

_LOGGER = logging.getLogger(__name__)


CODEOWNERS = ["@esphome/core"]
AUTO_LOAD = ["md5", "socket"]
DEPENDENCIES = ["network"]

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

    for port_conf in merged_ota_esphome_configs_by_port.values():
        new_ota_conf.append(port_conf)

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


CONFIG_SCHEMA = (
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
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = ota_esphome_final_validate


@coroutine_with_priority(52.0)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await ota_to_code(var, config)
    cg.add(var.set_port(config[CONF_PORT]))
    if CONF_PASSWORD in config:
        cg.add(var.set_auth_password(config[CONF_PASSWORD]))
        cg.add_define("USE_OTA_PASSWORD")
    cg.add_define("USE_OTA_VERSION", config[CONF_VERSION])

    await cg.register_component(var, config)
