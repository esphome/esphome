import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE
from esphome.components.esp32 import add_idf_sdkconfig_option
from esphome.const import CONF_WEB_SERVER, CONF_OTA

CODEOWNERS = ["@dentra"]

CONFIG_SCHEMA = cv.All(
    cv.Schema({}),
    cv.only_with_esp_idf,
)


async def to_code(config):
    # Increase the maximum supported size of headers section in HTTP request packet to be processed by the server
    add_idf_sdkconfig_option("CONFIG_HTTPD_MAX_REQ_HDR_LEN", 1024)

    if CONF_WEB_SERVER in CORE.config and CONF_OTA in CORE.config[CONF_WEB_SERVER]:
        cg.add_define("USE_WEBSERVER_IDF_MULTIPART")
