import esphome.codegen as cg
from esphome.components import ota, update
import esphome.config_validation as cv
from esphome.const import CONF_ESPHOME, CONF_PROJECT, CONF_SOURCE, CONF_VERSION
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import CONF_HTTP_REQUEST_ID, HttpRequestComponent, http_request_ns
from ..ota import OtaHttpRequestComponent

AUTO_LOAD = ["json"]
CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["ota.http_request"]

HttpRequestUpdate = http_request_ns.class_(
    "HttpRequestUpdate", update.UpdateEntity, cg.PollingComponent
)

CONF_OTA_ID = "ota_id"
CONF_ALLOW_AUTO_INSTALL = "allow_auto_install"

CONFIG_SCHEMA = (
    update.update_schema(HttpRequestUpdate)
    .extend(
        {
            cv.GenerateID(CONF_OTA_ID): cv.use_id(OtaHttpRequestComponent),
            cv.GenerateID(CONF_HTTP_REQUEST_ID): cv.use_id(HttpRequestComponent),
            cv.Required(CONF_SOURCE): cv.url,
            cv.Optional(CONF_ALLOW_AUTO_INSTALL, default=False): cv.boolean,
        }
    )
    .extend(cv.polling_component_schema("6h"))
)


def _final_validate(config: ConfigType) -> None:
    if not config[CONF_ALLOW_AUTO_INSTALL]:
        return
    # The check compares the manifest version with the project version, so the
    # fallback to the ESPHome version would make every manifest look different.
    if CONF_PROJECT not in fv.full_config.get()[CONF_ESPHOME]:
        raise cv.Invalid(
            f"'{CONF_ALLOW_AUTO_INSTALL}' requires a '{CONF_PROJECT}' with a "
            f"'{CONF_VERSION}' in the '{CONF_ESPHOME}' section, because the "
            "manifest version is compared with it.",
            path=[CONF_ALLOW_AUTO_INSTALL],
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = await update.new_update(config)
    await cg.add_variable(var.set_ota_parent, config[CONF_OTA_ID])
    await cg.add_variable(var.set_request_parent, config[CONF_HTTP_REQUEST_ID])

    cg.add(var.set_allow_auto_install(config[CONF_ALLOW_AUTO_INSTALL]))
    cg.add(var.set_source_url_static(config[CONF_SOURCE]))

    ota.request_ota_component_state_listeners()

    await cg.register_component(var, config)
