from esphome import automation
import esphome.codegen as cg
from esphome.components.ota import BASE_OTA_SCHEMA, OTAComponent, ota_to_code
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PASSWORD, CONF_URL, CONF_USERNAME
from esphome.core import coroutine_with_priority
from esphome.coroutine import CoroPriority
from esphome.types import ConfigType

from .. import CONF_HTTP_REQUEST_ID, HttpRequestComponent, http_request_ns

CODEOWNERS = ["@oarcher"]

AUTO_LOAD = ["md5"]
DEPENDENCIES = ["network", "http_request"]

CONF_MD5 = "md5"
CONF_MD5_URL = "md5_url"

OtaHttpRequestComponent = http_request_ns.class_(
    "OtaHttpRequestComponent", OTAComponent
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(OtaHttpRequestComponent),
            cv.GenerateID(CONF_HTTP_REQUEST_ID): cv.use_id(HttpRequestComponent),
        }
    )
    .extend(BASE_OTA_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    cv.require_framework_version(
        esp8266_arduino=cv.Version(2, 5, 1),
        esp32_arduino=cv.Version(0, 0, 0),
        esp_idf=cv.Version(0, 0, 0),
        rp2_arduino=cv.Version(0, 0, 0),
    ),
)


@coroutine_with_priority(CoroPriority.OTA_UPDATES)
async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await ota_to_code(var, config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_HTTP_REQUEST_ID])


OTA_HTTP_REQUEST_FLASH_ACTION_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(OtaHttpRequestComponent),
            cv.Optional(CONF_MD5_URL): cv.templatable(cv.url),
            cv.Optional(CONF_MD5): cv.templatable(
                cv.All(cv.string, cv.Length(min=32, max=32))
            ),
            cv.Optional(CONF_PASSWORD): cv.sensitive(cv.templatable(cv.string)),
            cv.Optional(CONF_USERNAME): cv.templatable(cv.string),
            cv.Required(CONF_URL): cv.templatable(cv.url),
        }
    ),
    cv.has_exactly_one_key(CONF_MD5, CONF_MD5_URL),
)


automation.register_apply_action(
    "ota.http_request.flash",
    OTA_HTTP_REQUEST_FLASH_ACTION_SCHEMA,
    automation.ApplyField(CONF_MD5_URL, "set_md5_url", cg.std_string),
    automation.ApplyField(CONF_MD5, "set_md5", cg.std_string),
    automation.ApplyField(CONF_PASSWORD, "set_password", cg.std_string),
    automation.ApplyField(CONF_USERNAME, "set_username", cg.std_string),
    automation.ApplyField(CONF_URL, "set_url", cg.std_string),
    automation.ApplyCall("flash()"),
)
