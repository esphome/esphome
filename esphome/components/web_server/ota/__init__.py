import logging

import esphome.codegen as cg
from esphome.components.esp32 import add_idf_component
from esphome.components.ota import (
    BASE_OTA_SCHEMA,
    DOMAIN as OTA_DOMAIN,
    OTAComponent,
    ota_to_code,
)
from esphome.components.wifi import CONF_AP_TIMEOUT, DEFAULT_AP_TIMEOUT_PERIOD
from esphome.config_helpers import merge_config
import esphome.config_validation as cv
from esphome.const import CONF_AP, CONF_ID, CONF_PLATFORM, CONF_WIFI
from esphome.core import CORE, coroutine_with_priority
from esphome.coroutine import CoroPriority
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import DOMAIN

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["network", "web_server_base"]

web_server_ns = cg.esphome_ns.namespace("web_server")
WebServerOTAComponent = web_server_ns.class_("WebServerOTAComponent", OTAComponent)

_HTTP_SERVER_PROVIDERS = (DOMAIN, "prometheus")
_HTTP_SERVER_PROVIDER_MESSAGE = ", ".join(
    f"'{provider}:'" for provider in _HTTP_SERVER_PROVIDERS
)


def _http_server_available(full_conf: ConfigType) -> bool:
    """Whether a configured component will start the shared HTTP server."""
    if any(provider in full_conf for provider in _HTTP_SERVER_PROVIDERS):
        return True
    if "captive_portal" not in full_conf:
        return False
    wifi_conf = full_conf.get(CONF_WIFI, {})
    ap_conf = wifi_conf.get(CONF_AP)
    if ap_conf is None:
        return False
    # A zero timeout can permanently disable the portal after provisioning when
    # saved station credentials are restored, even for an AP-only YAML config.
    return (
        ap_conf.get(CONF_AP_TIMEOUT, DEFAULT_AP_TIMEOUT_PERIOD).total_milliseconds != 0
    )


def _web_server_ota_final_validate(config: ConfigType) -> None:
    """Require an HTTP server and merge multiple web_server OTA instances into one.

    Multiple web_server OTA instances register duplicate HTTP handlers for /update,
    causing undefined behavior. Merge them into a single instance.
    """
    full_conf = fv.full_config.get()
    ota_confs = full_conf.get(OTA_DOMAIN, [])

    web_server_ota_configs: list[ConfigType] = []
    other_ota_configs: list[ConfigType] = []

    for ota_conf in ota_confs:
        if ota_conf.get(CONF_PLATFORM) == DOMAIN:
            web_server_ota_configs.append(ota_conf)
        else:
            other_ota_configs.append(ota_conf)

    # captive_portal auto-loads this platform, so with another OTA platform present a portal
    # that has no access point is only pointless, not a device without updates; captive_portal
    # already warns about the missing access point
    if not _http_server_available(full_conf) and not (
        "captive_portal" in full_conf and other_ota_configs
    ):
        raise cv.Invalid(
            "The web_server OTA platform requires a component that starts the HTTP "
            f"server: {_HTTP_SERVER_PROVIDER_MESSAGE}, "
            "or 'captive_portal:' with a "
            f"'{CONF_WIFI}.{CONF_AP}'"
        )

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
    full_conf[OTA_DOMAIN] = other_ota_configs
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
async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await ota_to_code(var, config)
    await cg.register_component(var, config)
    cg.add_define("USE_WEBSERVER_OTA")
    if CORE.is_esp32:
        add_idf_component(name="zorxx/multipart-parser", ref="1.0.1")
