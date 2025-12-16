import logging

import esphome.codegen as cg
from esphome.components.esp32 import add_idf_component
from esphome.components.ota import BASE_OTA_SCHEMA, OTAComponent, ota_to_code
from esphome.config_helpers import merge_config
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_OTA, CONF_PLATFORM, CONF_WEB_SERVER
from esphome.core import CORE, coroutine_with_priority
from esphome.coroutine import CoroPriority
import esphome.final_validate as fv
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["network", "web_server_base"]

web_server_ns = cg.esphome_ns.namespace("web_server")
WebServerOTAComponent = web_server_ns.class_("WebServerOTAComponent", OTAComponent)


def _web_server_ota_final_validate(config: ConfigType) -> None:
    """Merge multiple web_server OTA instances into one.

    Multiple web_server OTA instances register duplicate HTTP handlers for /update,
    causing undefined behavior. Merge them into a single instance.
    """
    full_conf = fv.full_config.get()
    ota_confs = full_conf.get(CONF_OTA, [])

    web_server_ota_configs: list[ConfigType] = []
    other_ota_configs: list[ConfigType] = []

    for ota_conf in ota_confs:
        if ota_conf.get(CONF_PLATFORM) == CONF_WEB_SERVER:
            web_server_ota_configs.append(ota_conf)
        else:
            other_ota_configs.append(ota_conf)

    if len(web_server_ota_configs) <= 1:
        return

    # Merge all web_server OTA configs into the first one
    merged = web_server_ota_configs[0]
    for ota_conf in web_server_ota_configs[1:]:
        # Validate that IDs are consistent if manually specified
        if (
            merged[CONF_ID].is_manual
            and ota_conf[CONF_ID].is_manual
            and merged[CONF_ID] != ota_conf[CONF_ID]
        ):
            raise cv.Invalid(
                f"Found multiple web_server OTA configurations but {CONF_ID} is inconsistent"
            )
        merged = merge_config(merged, ota_conf)

    _LOGGER.warning(
        "Found and merged %d web_server OTA configurations into one instance",
        len(web_server_ota_configs),
    )

    # Replace OTA configs with merged web_server + other OTA platforms
    other_ota_configs.append(merged)
    full_conf[CONF_OTA] = other_ota_configs
    fv.full_config.set(full_conf)


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(WebServerOTAComponent),
        }
    )
    .extend(BASE_OTA_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = _web_server_ota_final_validate


@coroutine_with_priority(CoroPriority.WEB_SERVER_OTA)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await ota_to_code(var, config)
    await cg.register_component(var, config)
    cg.add_define("USE_WEBSERVER_OTA")
    if CORE.is_esp32:
        add_idf_component(name="zorxx/multipart-parser", ref="1.0.1")
