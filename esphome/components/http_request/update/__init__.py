import esphome.codegen as cg
from esphome.components import update
import esphome.config_validation as cv
from esphome.const import CONF_SOURCE

from .. import CONF_HTTP_REQUEST_ID, HttpRequestComponent, http_request_ns
from ..ota import OtaHttpRequestComponent

AUTO_LOAD = ["json"]
CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["ota.http_request"]

HttpRequestUpdate = http_request_ns.class_(
    "HttpRequestUpdate", update.UpdateEntity, cg.PollingComponent
)

CONF_OTA_ID = "ota_id"
CONF_CHECK_UPDATE = "check_update"

CONFIG_SCHEMA = update.UPDATE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(HttpRequestUpdate),
        cv.GenerateID(CONF_OTA_ID): cv.use_id(OtaHttpRequestComponent),
        cv.GenerateID(CONF_HTTP_REQUEST_ID): cv.use_id(HttpRequestComponent),
        cv.Required(CONF_SOURCE): cv.url,
        cv.Optional(CONF_CHECK_UPDATE): cv.lambda_,
    }
).extend(cv.polling_component_schema("6h"))


async def to_code(config):
    var = await update.new_update(config)
    ota_parent = await cg.get_variable(config[CONF_OTA_ID])
    cg.add(var.set_ota_parent(ota_parent))
    request_parent = await cg.get_variable(config[CONF_HTTP_REQUEST_ID])
    cg.add(var.set_request_parent(request_parent))

    cg.add(var.set_source_url(config[CONF_SOURCE]))

    if CONF_CHECK_UPDATE in config:
        lambda_ = await cg.process_lambda(
            config[CONF_CHECK_UPDATE],
            [(update.UpdateInfo.operator("ref").operator("const"), "x")],
            return_type=cg.bool_,
            capture="",
        )
        cg.add(var.set_check_update(lambda_))

    cg.add_define("USE_OTA_STATE_CALLBACK")

    await cg.register_component(var, config)
