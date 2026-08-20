from esphome.components.esp32 import add_idf_sdkconfig_option
import esphome.config_validation as cv
from esphome.core import CORE

CODEOWNERS = ["@dentra"]

CONFIG_SCHEMA = cv.All(
    cv.Schema({}),
    cv.only_on_esp32,
)


async def to_code(config):
    # Increase the maximum supported size of headers section in HTTP request packet to be processed by the server
    add_idf_sdkconfig_option("CONFIG_HTTPD_MAX_REQ_HDR_LEN", 1024)


def FILTER_SOURCE_FILES() -> list[str]:
    # multipart.cpp is fully #ifdef'd on USE_WEBSERVER_OTA (set by the
    # web_server OTA platform); skip it when OTA uploads are not configured.
    if not any(define.name == "USE_WEBSERVER_OTA" for define in CORE.defines):
        return ["multipart.cpp"]
    return []
