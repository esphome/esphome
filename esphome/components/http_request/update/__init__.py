import esphome.codegen as cg
from esphome.components import update
import esphome.config_validation as cv
from esphome.const import CONF_SOURCE
from esphome.types import ConfigType

from .. import CONF_HTTP_REQUEST_ID, HttpRequestComponent, http_request_ns

AUTO_LOAD = ["json"]
CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["http_request"]

HttpRequestUpdate = http_request_ns.class_(
    "HttpRequestUpdate", update.UpdateEntity, cg.PollingComponent
)

# Interface for components that install an image announced by an update
# manifest. See update_target.h.
UpdateTarget = http_request_ns.class_("UpdateTarget")

CONF_OTA_ID = "ota_id"
CONF_TARGET_ID = "target_id"

CONFIG_SCHEMA = cv.All(
    cv.rename_key(
        CONF_OTA_ID,
        CONF_TARGET_ID,
        removed_in="2027.5.0",
        component="update.http_request",
    ),
    update.update_schema(HttpRequestUpdate)
    .extend(
        {
            cv.GenerateID(CONF_TARGET_ID): cv.use_id(UpdateTarget),
            cv.GenerateID(CONF_HTTP_REQUEST_ID): cv.use_id(HttpRequestComponent),
            cv.Required(CONF_SOURCE): cv.url,
        }
    )
    .extend(cv.polling_component_schema("6h")),
)


async def to_code(config: ConfigType) -> None:
    var = await update.new_update(config)

    # Lets a target compile in the code that reports install progress back here.
    cg.add_define("USE_HTTP_REQUEST_UPDATE")

    await cg.add_variable(var.set_target, config[CONF_TARGET_ID])
    await cg.add_variable(var.set_request_parent, config[CONF_HTTP_REQUEST_ID])

    cg.add(var.set_source_url_static(config[CONF_SOURCE]))

    await cg.register_component(var, config)
